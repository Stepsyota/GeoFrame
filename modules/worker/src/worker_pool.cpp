#include "worker/worker_pool.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace geoframe::worker {

WorkerPool::WorkerPool(core::IJobRepository& jobs, IJobHandler& handler,
                       const std::size_t thread_count,
                       const std::chrono::milliseconds poll_interval)
    : jobs(jobs),
      handler(handler),
      thread_count(thread_count),
      poll_interval(poll_interval) {
    if (thread_count == 0) {
        throw std::invalid_argument("Worker pool must contain at least one thread");
    }
}

WorkerPool::~WorkerPool() {
    stop();
}

void WorkerPool::start() {
    if (!threads.empty()) {
        throw std::logic_error("Worker pool is already running");
    }

    jobs.recover_interrupted();
    {
        std::lock_guard lock{state_mutex};
        fatal_error = nullptr;
    }

    threads.reserve(thread_count);
    for (std::size_t index = 0; index < thread_count; ++index) {
        threads.emplace_back([this](const std::stop_token stop_token) { run(stop_token); });
    }
}

void WorkerPool::stop() {
    for (auto& thread : threads) {
        thread.request_stop();
    }
    wakeup.notify_all();
    threads.clear();
}

void WorkerPool::rethrow_if_failed() const {
    std::lock_guard lock{state_mutex};
    if (fatal_error) {
        std::rethrow_exception(fatal_error);
    }
}

void WorkerPool::run(const std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::optional<core::Job> job;
        try {
            job = jobs.claim_next();
        } catch (...) {
            save_fatal_error(std::current_exception());
            return;
        }

        if (!job.has_value()) {
            std::unique_lock lock{state_mutex};
            wakeup.wait_for(lock, poll_interval,
                            [&] { return stop_token.stop_requested(); });
            continue;
        }

        try {
            handler.execute(*job);
            jobs.mark_done(job->id);
        } catch (const std::exception& error) {
            try {
                jobs.mark_failed(job->id, error.what());
            } catch (...) {
                save_fatal_error(std::current_exception());
                return;
            }
        } catch (...) {
            try {
                jobs.mark_failed(job->id, "Unknown worker error");
            } catch (...) {
                save_fatal_error(std::current_exception());
                return;
            }
        }
    }
}

void WorkerPool::save_fatal_error(std::exception_ptr error) {
    std::lock_guard lock{state_mutex};
    if (!fatal_error) {
        fatal_error = std::move(error);
    }
}

}  // namespace geoframe::worker
