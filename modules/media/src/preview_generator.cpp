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

    const std::string size = std::to_string(max_size);
    const std::string filter =
        "scale=w='min(" + size + ",iw)':h='min(" + size +
        ",ih)':force_original_aspect_ratio=decrease";
    const std::vector<std::string> arguments{
        ffmpeg_binary, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
        "-i",          source.string(),  "-vf",       filter,  "-frames:v", "1",
        "-q:v",        "3",              temporary.string(),
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
