#pragma once

#include <memory>
#include <string>

#include "storage/storage_interface.hpp"

namespace sw::redis {
class Redis;
}

namespace hydra::storage {

class RedisStorage final : public IStorageBackend {
public:
    RedisStorage() = default;

    ~RedisStorage() override;

    bool connect(const std::string& host, int port) noexcept override;

    void flush_snapshot(const core::MetricSnapshot& snapshot) noexcept override;

private:
    std::unique_ptr<sw::redis::Redis> redis_;
};

}  // namespace hydra::storage
