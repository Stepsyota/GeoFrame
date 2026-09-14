#include "media/process_runner.hpp"

#include <reproc++/reproc.hpp>
#include <reproc++/run.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace geoframe::media {

ProcessResult run_process(const std::vector<std::string>& arguments,
                          const std::chrono::milliseconds timeout) {
    if (arguments.empty()) {
        throw std::invalid_argument("Process arguments cannot be empty");
    }
    if (timeout.count() <= 0 ||
        timeout.count() > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Process timeout is outside supported range");
    }

    reproc::options options;
    options.deadline = reproc::milliseconds{static_cast<int>(timeout.count())};
    options.stop = {
        {reproc::stop::terminate, reproc::milliseconds{1'000}},
        {reproc::stop::kill, reproc::milliseconds{1'000}},
        {reproc::stop::kill, reproc::milliseconds{1'000}},
    };

    std::string output;
    std::string error;
    const auto [exit_code, process_error] =
        reproc::run(arguments, options, reproc::sink::string{output}, reproc::sink::string{error});
    if (process_error) {
        throw std::runtime_error("Cannot run process: " + process_error.message());
    }

    return ProcessResult{
        .exit_code = exit_code,
        .output = std::move(output),
        .error = std::move(error),
    };
}

}  // namespace geoframe::media
