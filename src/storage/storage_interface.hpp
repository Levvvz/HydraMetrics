#pragma once

#include <string>

#include "core/aggregator_interface.hpp"

namespace hydra::storage {

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual bool connect(const std::string& host, int port) = 0;

    virtual void flush_snapshot(const core::MetricSnapshot& snapshot) = 0;
};

}  // namespace hydra::storage
