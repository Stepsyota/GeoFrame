#include "media/preview_generator.hpp"

#include "media/process_runner.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace geoframe::media {

namespace {

/** Publish tmp → destination atomically; removes tmp on error. */
void publish(const std::filesystem::path& tmp, const std::filesystem::path& destination) {
    std::error_code ec;
    std::filesystem::remove(destination, ec);
    ec.clear();
    std::filesystem::rename(tmp, destination, ec);
    if (ec) {
        std::filesystem::remove(tmp);
        throw std::runtime_error("Cannot publish generated preview: " + ec.message());
    }
}

void remove_tmp(const std::filesystem::path& tmp) noexcept {
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

/** Scale filter: limit the long edge to max_size, keep aspect ratio. */
std::string scale_filter_complex(const int max_size) {
    const std::string s = std::to_string(max_size);
    return "[0:v]scale='if(gt(iw,ih)," + s + ",-2)':'if(gt(iw,ih),-2," + s + ")'[out]";
}

bool is_heif_file(const std::filesystem::path& source) {
    auto ext = source.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".heic" || ext == ".heif" || ext == ".avif";
}

// ── ffmpeg scale (works on already-decoded JPEG/PNG/etc.) ─────────────────

bool try_ffmpeg_scale(const std::filesystem::path& source, const std::filesystem::path& tmp,
                      const int max_size, const std::string& ffmpeg_binary) {
    const std::vector<std::string> args{
        ffmpeg_binary,
        "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
        "-i", source.string(),
        "-filter_complex", scale_filter_complex(max_size),
        "-map", "[out]",
        "-frames:v", "1",
        "-update", "1",
        "-q:v", "3",
        tmp.string(),
    };

    const auto result = run_process(args, std::chrono::minutes{3});
    if (result.exit_code != 0) {
        std::clog << "[preview] ffmpeg scale failed for " << source.filename().string()
                  << ": " << result.error << '\n';
        remove_tmp(tmp);
        return false;
    }
    return true;
}

// ── HEIF/HEIC: libheif decode → ffmpeg scale ─────────────────────────────
//
// ffmpeg's HEIC demuxer reads the tile-grid stream and produces a cropped,
// zoomed fragment.  heif-convert assembles the full image correctly.

bool try_heif_convert(const std::filesystem::path& source, const std::filesystem::path& tmp,
                      const int max_size) {
    auto decoded = tmp;
    decoded.replace_filename(tmp.stem().string() + ".decode.jpg");

    const std::vector<std::string> decode_args{
        "heif-convert",
        source.string(),
        decoded.string(),
    };

    const auto decode = run_process(decode_args, std::chrono::minutes{3});
    if (decode.exit_code != 0) {
        std::clog << "[preview] heif-convert failed for " << source.filename().string()
                  << ": " << decode.error << '\n';
        remove_tmp(decoded);
        return false;
    }

    const bool scaled = try_ffmpeg_scale(decoded, tmp, max_size, "ffmpeg");
    remove_tmp(decoded);
    return scaled;
}

// ── ImageMagick (magick / convert) ────────────────────────────────────────

bool try_imagemagick(const std::filesystem::path& source, const std::filesystem::path& tmp,
                     const int max_size) {
    const std::string geometry = std::to_string(max_size) + "x" + std::to_string(max_size) + ">";

    std::string binary = "magick";
    {
        const auto probe = run_process({"magick", "--version"}, std::chrono::seconds{5});
        if (probe.exit_code != 0) {
            binary = "convert";
        }
    }

    const std::vector<std::string> args{
        binary,
        "-auto-orient",
        source.string() + "[0]",
        "-resize", geometry,
        "-quality", "85",
        "jpg:" + tmp.string(),
    };

    const auto result = run_process(args, std::chrono::minutes{3});
    if (result.exit_code != 0) {
        std::clog << "[preview] convert failed for " << source.filename().string()
                  << ": " << result.error << '\n';
        remove_tmp(tmp);
        return false;
    }
    return true;
}

// ── vips thumbnail ───────────────────────────────────────────────────────

bool try_vips(const std::filesystem::path& source, const std::filesystem::path& tmp,
              const int max_size) {
    const std::vector<std::string> args{
        "vips", "thumbnail",
        source.string(),
        tmp.string(),
        std::to_string(max_size),
        "--size", "down",
    };

    const auto result = run_process(args, std::chrono::minutes{3});
    if (result.exit_code != 0) {
        std::clog << "[preview] vips failed for " << source.filename().string()
                  << ": " << result.error << '\n';
        remove_tmp(tmp);
        return false;
    }
    return true;
}

}  // namespace

void generate_image_preview(const std::filesystem::path& source,
                            const std::filesystem::path& destination, const int max_size,
                            const std::string& ffmpeg_binary) {
    if (max_size <= 0) {
        throw std::invalid_argument("Preview max size must be positive");
    }
    if (!std::filesystem::is_regular_file(source)) {
        throw std::invalid_argument("Preview source is not a regular file: " + source.string());
    }
    if (!destination.is_absolute()) {
        throw std::invalid_argument("Preview destination must be absolute");
    }

    std::filesystem::create_directories(destination.parent_path());

    auto tmp = destination;
    tmp.replace_filename(destination.stem().string() + ".tmp.jpg");

    // HEIC/HEIF: ffmpeg tile-grid decode is broken — use libheif first.
    if (is_heif_file(source)) {
        if (try_heif_convert(source, tmp, max_size)) {
            publish(tmp, destination);
            return;
        }
        std::clog << "[preview] heif-convert failed for " << source.filename().string()
                  << "; trying fallbacks\n";
    }

    if (try_ffmpeg_scale(source, tmp, max_size, ffmpeg_binary)) {
        publish(tmp, destination);
        return;
    }

    if (try_imagemagick(source, tmp, max_size)) {
        publish(tmp, destination);
        return;
    }

    if (try_vips(source, tmp, max_size)) {
        publish(tmp, destination);
        return;
    }

    throw std::runtime_error(
        "All preview tools (heif-convert/ffmpeg, convert, vips) failed for: " + source.string());
}

}  // namespace geoframe::media
