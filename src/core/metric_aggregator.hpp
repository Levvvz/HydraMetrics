#pragma once

#include <array>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

#include "aggregator_interface.hpp"

namespace hydra::core {

class MetricAggregator final : public IMetricAggregator {
public:
    void update_metric(const MetricPacket& packet) noexcept override;

    MetricSnapshot extract_snapshot() override;

private:
    static constexpr size_t SHARD_COUNT = 16;

    struct BucketShard {
        std::unordered_map<std::string, double> counters;
        std::unordered_map<std::string, double> gauges;
        std::mutex mutex;
    };

    size_t get_shard_index(const std::string& name) const noexcept;

    std::array<BucketShard, SHARD_COUNT> shards_;
};

}  // namespace hydra::core
