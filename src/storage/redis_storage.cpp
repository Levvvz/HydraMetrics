#include "storage/redis_storage.hpp"

#include <sw/redis++/redis++.h>

#include <iostream>
#include <utility>

namespace hydra::storage {
namespace {

constexpr const char* COUNTERS_HASH_KEY = "hydra:counters";
constexpr const char* GAUGES_HASH_KEY = "hydra:gauges";

}  // namespace

RedisStorage::RedisStorage() = default;

RedisStorage::~RedisStorage() = default;

bool RedisStorage::connect(const std::string& host, int port) noexcept {
    try {
        sw::redis::ConnectionOptions options;
        options.host = host;
        options.port = port;

        auto redis = std::make_unique<sw::redis::Redis>(options);
        redis->ping();
        redis_ = std::move(redis);

        return true;
    } catch (const sw::redis::Error& error) {
        std::cerr << "[Redis Error]: " << error.what() << std::endl;
        redis_.reset();
        return false;
    } catch (...) {
        std::cerr << "[Redis Error]: unexpected failure while connecting" << std::endl;
        redis_.reset();
        return false;
    }
}

void RedisStorage::flush_snapshot(const core::MetricSnapshot& snapshot) noexcept {
    if (!redis_) {
        return;
    }

    try {
        auto pipeline = redis_->pipeline();

        for (const auto& [name, value] : snapshot.counters) {
            pipeline.hincrbyfloat(COUNTERS_HASH_KEY, name, value);
        }
        for (const auto& [name, value] : snapshot.gauges) {
            pipeline.hset(GAUGES_HASH_KEY, name, std::to_string(value));
        }

        pipeline.exec();
    } catch (const sw::redis::Error& error) {
        std::cerr << "[Redis Error]: " << error.what() << std::endl;
    } catch (...) {
        std::cerr << "[Redis Error]: unexpected failure while flushing snapshot" << std::endl;
    }
}

}  // namespace hydra::storage
