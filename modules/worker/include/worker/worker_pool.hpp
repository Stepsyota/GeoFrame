#pragma once

#include "core/job_repository.hpp"
#include "worker/job_handler.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace geoframe::worker {

/**
 * @brief Выполняет задачи из персистентной очереди в нескольких потоках.
 */
class WorkerPool {
public:
    WorkerPool(core::IJobRepository& jobs, IJobHandler& handler, std::size_t thread_count,
               std::chrono::milliseconds poll_interval = std::chrono::milliseconds{100});
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void start();
    void stop();
    void rethrow_if_failed() const;

private:
    void run(std::stop_token stop_token);
    void save_fatal_error(std::exception_ptr error);

    core::IJobRepository& jobs;
    IJobHandler& handler;
    std::size_t thread_count;
    std::chrono::milliseconds poll_interval;
    std::vector<std::jthread> threads;
    mutable std::mutex state_mutex;
    std::condition_variable wakeup;
    std::exception_ptr fatal_error;
};

}  // namespace geoframe::worker
