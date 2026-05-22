#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "core/aggregator_interface.hpp"
#include "storage/storage_interface.hpp"

namespace hydra::storage {

class FlushWorker final {
public:
    FlushWorker(core::IMetricAggregator& aggregator, IStorageBackend& storage) noexcept;

    ~FlushWorker() noexcept;

    FlushWorker(const FlushWorker&) = delete;
    FlushWorker& operator=(const FlushWorker&) = delete;
    FlushWorker(FlushWorker&&) = delete;
    FlushWorker& operator=(FlushWorker&&) = delete;

    void start(std::chrono::milliseconds interval) noexcept;

    void stop() noexcept;

private:
    void run(std::stop_token stop_token, std::chrono::milliseconds interval) noexcept;

    core::IMetricAggregator& aggregator_;
    IStorageBackend& storage_;
    std::mutex lifecycle_mutex_;
    std::condition_variable_any wakeup_;
    std::jthread worker_;
};

}  // namespace hydra::storage
