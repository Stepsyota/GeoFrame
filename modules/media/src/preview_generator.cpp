#include "media/preview_generator.hpp"

#include "media/process_runner.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace geoframe::media {

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
    auto temporary = destination;
    temporary.replace_filename(destination.stem().string() + ".tmp.jpg");

    // Use -filter_complex instead of -vf so the filter is compatible with
    // HEIC/HEIF inputs: modern ffmpeg (≥5.x) creates an internal complex
    // filtergraph for HEIC decoding, which conflicts with -vf (simple filter).
    //
    // Scale: if landscape → width=max_size, height proportional (-2);
    //        if portrait  → height=max_size, width proportional (-2).
    // -2 means "make divisible by 2 and proportional" (required by some codecs).
    const std::string s = std::to_string(max_size);
    const std::string filter_complex =
        "[0:v]scale='if(gt(iw,ih)," + s + ",-2)':'if(gt(iw,ih),-2," + s + ")'[out]";

    const std::vector<std::string> arguments{
        ffmpeg_binary,
        "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
        "-i", source.string(),
        "-filter_complex", filter_complex,
        "-map", "[out]",
        "-frames:v", "1",
        "-q:v", "3",
        temporary.string(),
    };

    const auto result = run_process(arguments, std::chrono::minutes{5});
    if (result.exit_code != 0) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("ffmpeg cannot generate preview: " + result.error);
    }

    std::error_code error;
    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot publish generated preview: " + error.message());
    }
}

}  // namespace geoframe::media
