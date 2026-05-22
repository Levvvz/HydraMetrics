#include "core/metric_aggregator.hpp"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "core/types.hpp"

namespace hydra::core {
namespace {

TEST(MetricAggregatorTest, SingleThreadedUpdate) {
    MetricAggregator aggregator;

    aggregator.update_metric({MetricType::Counter, 1, "requests", 2.0});
    aggregator.update_metric({MetricType::Counter, 2, "requests", 3.0});
    aggregator.update_metric({MetricType::Counter, 3, "errors", 1.0});
    aggregator.update_metric({MetricType::Gauge, 4, "temperature", 21.5});
    aggregator.update_metric({MetricType::Gauge, 5, "temperature", 22.0});
    aggregator.update_metric({MetricType::Gauge, 6, "load", 0.75});

    const MetricSnapshot first_snapshot = aggregator.extract_snapshot();

    EXPECT_EQ(first_snapshot.counters.at("requests"), 5.0);
    EXPECT_EQ(first_snapshot.counters.at("errors"), 1.0);
    EXPECT_EQ(first_snapshot.gauges.at("temperature"), 22.0);
    EXPECT_EQ(first_snapshot.gauges.at("load"), 0.75);

    const MetricSnapshot second_snapshot = aggregator.extract_snapshot();

    EXPECT_EQ(second_snapshot.counters.count("requests"), 0U);
    EXPECT_EQ(second_snapshot.counters.count("errors"), 0U);
    EXPECT_EQ(second_snapshot.gauges.at("temperature"), 22.0);
    EXPECT_EQ(second_snapshot.gauges.at("load"), 0.75);
}

TEST(MetricAggregatorTest, ConcurrentStressTest) {
    MetricAggregator aggregator;
    constexpr size_t thread_count = 8;
    constexpr size_t iterations_per_thread = 10000;

    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([&aggregator]() {
            const MetricPacket packet{MetricType::Counter, 1, "concurrent_counter", 1.0};

            for (size_t iteration = 0; iteration < iterations_per_thread; ++iteration) {
                aggregator.update_metric(packet);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    const MetricSnapshot snapshot = aggregator.extract_snapshot();
    const double expected_value = static_cast<double>(thread_count * iterations_per_thread);

    ASSERT_EQ(snapshot.counters.at("concurrent_counter"), expected_value);
}

}  // namespace
}  // namespace hydra::core
