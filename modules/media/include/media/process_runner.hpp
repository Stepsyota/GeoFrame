#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace geoframe::media {

struct ProcessResult {
    int exit_code;
    std::string output;
    std::string error;
};

/**
 * @brief Запускает процесс без shell и захватывает stdout/stderr.
 */
ProcessResult run_process(const std::vector<std::string>& arguments,
                          std::chrono::milliseconds timeout);

}  // namespace geoframe::media
