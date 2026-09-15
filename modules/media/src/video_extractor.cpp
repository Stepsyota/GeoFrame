#include "media/video_extractor.hpp"

#include "media/preview_generator.hpp"
#include "media/process_runner.hpp"

#include <charconv>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

namespace geoframe::media {

namespace {

// Parse a simple key=value line from ffprobe compact output.
// Format: key=value\n
std::string_view parse_ffprobe_field(std::string_view output, std::string_view key) {
    const std::string prefix = std::string{key} + "=";
    std::size_t pos = 0;
    while (pos < output.size()) {
        const auto nl = output.find('\n', pos);
        const auto line = output.substr(pos, nl == std::string_view::npos ? nl : nl - pos);
        if (line.starts_with(prefix)) {
            return line.substr(prefix.size());
        }
        pos = (nl == std::string_view::npos ? output.size() : nl + 1);
    }
    return {};
}

std::optional<double> try_parse_double(std::string_view sv) {
    if (sv.empty()) {
        return std::nullopt;
    }
    // ffprobe may report "N/A"
    if (sv == "N/A" || sv == "n/a") {
        return std::nullopt;
    }
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> try_parse_int(std::string_view sv) {
    if (sv.empty()) {
        return std::nullopt;
    }
    int value = 0;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

// Convert ffprobe creation_time (ISO 8601) to "YYYY-MM-DDTHH:MM:SS"
std::optional<std::string> normalize_creation_time(std::string_view raw) {
    // ffprobe format: "2024-08-15T14:32:10.000000Z"
    if (raw.size() < 19) {
        return std::nullopt;
    }
    return std::string{raw.substr(0, 19)};
}

}  // namespace

core::VideoMetadata extract_video_metadata(const std::filesystem::path& path) {
    // Run ffprobe to get stream and format information
    // -show_entries stream=... -show_entries format=... gives us compact key=value output
    const std::vector<std::string> args = {
        "ffprobe",
        "-v", "quiet",
        "-print_format", "flat",
        "-show_streams",
        "-show_entries",
        "stream=codec_name,width,height:format=duration:format_tags=creation_time",
        path.string(),
    };

    constexpr auto kTimeout = std::chrono::milliseconds{15'000};
    const auto result = run_process(args, kTimeout);

    if (result.exit_code != 0) {
        throw std::runtime_error(
            "ffprobe failed for " + path.string() + ": " + result.error);
    }

    const std::string_view out = result.output;

    core::VideoMetadata meta;

    // Duration from format section: format.duration=12.345
    const auto dur_sv = parse_ffprobe_field(out, "format.duration");
    meta.duration_seconds = try_parse_double(dur_sv);

    // Codec from first video stream: streams.stream.0.codec_name=h264
    const auto codec_sv = parse_ffprobe_field(out, "streams.stream.0.codec_name");
    if (!codec_sv.empty()) {
        meta.codec = std::string{codec_sv};
    }

    // Width / height from first stream
    meta.width = try_parse_int(parse_ffprobe_field(out, "streams.stream.0.width"));
    meta.height = try_parse_int(parse_ffprobe_field(out, "streams.stream.0.height"));

    // Creation time (shooting date) from format tags
    const auto ct_sv = parse_ffprobe_field(out, "format.tags.creation_time");
    if (!ct_sv.empty()) {
        // ffprobe wraps string values in quotes in flat format
        auto stripped = ct_sv;
        if (stripped.size() >= 2 && stripped.front() == '"' && stripped.back() == '"') {
            stripped = stripped.substr(1, stripped.size() - 2);
        }
        meta.captured_at = normalize_creation_time(stripped);
    }

    return meta;
}

void generate_video_poster(const std::filesystem::path& source,
                           const std::filesystem::path& dest, const int max_px) {
    // Re-use the same ffmpeg-based preview generator:
    // for videos, extract frame at position 0 instead of scale-only.
    std::filesystem::create_directories(dest.parent_path());

    const auto tmp = dest.parent_path() / (dest.filename().string() + ".tmp.jpg");

    const std::string s = std::to_string(max_px);
    const std::string filter_complex =
        "[0:v]scale='if(gt(iw,ih)," + s + ",-2)':'if(gt(iw,ih),-2," + s + ")'[out]";

    const std::vector<std::string> args = {
        "ffmpeg",
        "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
        "-ss", "0",
        "-i", source.string(),
        "-filter_complex", filter_complex,
        "-map", "[out]",
        "-frames:v", "1",
        "-update", "1",
        "-q:v", "3",
        tmp.string(),
    };

    constexpr auto kTimeout = std::chrono::milliseconds{30'000};
    const auto result = run_process(args, kTimeout);

    if (result.exit_code != 0) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error(
            "ffmpeg poster generation failed for " + source.string() + ": " + result.error);
    }

    std::filesystem::rename(tmp, dest);
}

}  // namespace geoframe::media
