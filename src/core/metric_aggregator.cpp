#include "core/metric_aggregator.hpp"

#include <functional>

namespace hydra::core {

void MetricAggregator::update_metric(const MetricPacket& packet) noexcept {
    try {
        BucketShard& shard = shards_[get_shard_index(packet.name)];
        std::lock_guard<std::mutex> lock(shard.mutex);

        if (packet.type == MetricType::Counter) {
            shard.counters[packet.name] += packet.value;
        } else if (packet.type == MetricType::Gauge) {
            shard.gauges[packet.name] = packet.value;
        }
    } catch (...) {
    }
}

MetricSnapshot MetricAggregator::extract_snapshot() {
    MetricSnapshot snapshot;
    std::scoped_lock lock(shards_[0].mutex, shards_[1].mutex, shards_[2].mutex, shards_[3].mutex, shards_[4].mutex,
                          shards_[5].mutex, shards_[6].mutex, shards_[7].mutex, shards_[8].mutex, shards_[9].mutex,
                          shards_[10].mutex, shards_[11].mutex, shards_[12].mutex, shards_[13].mutex, shards_[14].mutex,
                          shards_[15].mutex);

    for (BucketShard& shard : shards_) {
        for (const auto& [name, value] : shard.counters) {
            snapshot.counters[name] += value;
        }
        for (const auto& [name, value] : shard.gauges) {
            snapshot.gauges[name] = value;
        }

        shard.counters.clear();
    }

    return snapshot;
}

size_t MetricAggregator::get_shard_index(const std::string& name) const noexcept {
    return std::hash<std::string>{}(name) % SHARD_COUNT;
}

}  // namespace hydra::core
