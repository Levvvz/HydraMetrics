#include "storage/flush_worker.hpp"

namespace hydra::storage {

FlushWorker::FlushWorker(core::IMetricAggregator& aggregator, IStorageBackend& storage) noexcept
    : aggregator_(aggregator), storage_(storage) {
}

FlushWorker::~FlushWorker() noexcept {
    stop();
}

void FlushWorker::start(std::chrono::milliseconds interval) noexcept {
    try {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (worker_.joinable()) {
            return;
        }

        worker_ = std::jthread([this, interval](std::stop_token stop_token) noexcept { run(stop_token, interval); });
    } catch (...) {
    }
}

void FlushWorker::stop() noexcept {
    try {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (worker_.joinable()) {
            worker_.request_stop();
            wakeup_.notify_all();
            worker_.join();
        }
    } catch (...) {
    }
}

void FlushWorker::run(std::stop_token stop_token, std::chrono::milliseconds interval) noexcept {
    while (!stop_token.stop_requested()) {
        std::mutex wait_mutex;
        std::unique_lock<std::mutex> wait_lock(wait_mutex);
        const bool should_stop = wakeup_.wait_for(wait_lock, stop_token, interval, [] { return false; });

        if (should_stop || stop_token.stop_requested()) {
            break;
        }

        try {
            core::MetricSnapshot snapshot = aggregator_.extract_snapshot();
            storage_.flush_snapshot(snapshot);
        } catch (...) {
        }
    }
}

}  // namespace hydra::storage
