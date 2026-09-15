#include "media/preview_generator.hpp"

#include "media/process_runner.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace geoframe::media {

namespace {

// ── Tool helpers ──────────────────────────────────────────────────────────

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

/** Cleans up leftover temp file without throwing. */
void remove_tmp(const std::filesystem::path& tmp) noexcept {
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

// ── Strategy 1: ffmpeg ───────────────────────────────────────────────────

bool try_ffmpeg(const std::filesystem::path& source, const std::filesystem::path& tmp,
                const int max_size, const std::string& ffmpeg_binary) {
    const std::string s   = std::to_string(max_size);
    // -filter_complex required for HEIC: modern ffmpeg uses an internal complex
    // filtergraph for HEVC-based formats, conflicting with -vf (simple filter).
    const std::string fc  =
        "[0:v]scale='if(gt(iw,ih)," + s + ",-2)':'if(gt(iw,ih),-2," + s + ")'[out]";

    const std::vector<std::string> args{
        ffmpeg_binary,
        "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
        "-i", source.string(),
        "-filter_complex", fc,
        "-map", "[out]",
        "-frames:v", "1",
        "-q:v", "3",
        tmp.string(),
    };

    const auto result = run_process(args, std::chrono::minutes{3});
    if (result.exit_code != 0) {
        std::clog << "[preview] ffmpeg failed for " << source.filename().string()
                  << ": " << result.error << '\n';
        remove_tmp(tmp);
        return false;
    }
    return true;
}

// ── Strategy 2: ImageMagick (magick / convert) ────────────────────────────
//
// Handles RAW formats (DNG, NEF, CR2, ARW, …) via dcraw delegate.
// ImageMagick 7 prefers the "magick" binary; if not found we fall back
// to the legacy "convert" alias (still present but shows a deprecation warning).

bool try_imagemagick(const std::filesystem::path& source, const std::filesystem::path& tmp,
                     const int max_size) {
    // '[0]' selects the first frame/layer (important for multi-image formats).
    // -auto-orient  — respect EXIF rotation.
    // WxH>          — shrink only if larger than the requested size.
    const std::string geometry = std::to_string(max_size) + "x" + std::to_string(max_size) + ">";

    // Prefer IM7 "magick" binary; older systems still ship "convert".
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
        source.string() + "[0]",  // first frame/layer
        "-resize", geometry,
        "-quality", "85",
        "jpg:" + tmp.string(),    // explicit format so extension doesn't matter
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

// ── Strategy 3: vips thumbnail ───────────────────────────────────────────

bool try_vips(const std::filesystem::path& source, const std::filesystem::path& tmp,
              const int max_size) {
    // vips thumbnail <src> <dst> <size>  (size = max of width/height)
    const std::vector<std::string> args{
        "vips", "thumbnail",
        source.string(),
        tmp.string(),
        std::to_string(max_size),
        "--size", "down",   // shrink only
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

// ── Public API ────────────────────────────────────────────────────────────

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

    if (try_ffmpeg(source, tmp, max_size, ffmpeg_binary)) {
        publish(tmp, destination);
        return;
    }

    std::clog << "[preview] ffmpeg failed for " << source.filename().string()
              << "; trying ImageMagick convert\n";

    if (try_imagemagick(source, tmp, max_size)) {
        publish(tmp, destination);
        return;
    }

    std::clog << "[preview] convert failed for " << source.filename().string()
              << "; trying vips\n";

    if (try_vips(source, tmp, max_size)) {
        publish(tmp, destination);
        return;
    }

    throw std::runtime_error(
        "All preview tools (ffmpeg, convert, vips) failed for: " + source.string());
}

}  // namespace geoframe::media
