#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#include "core/metric_aggregator.hpp"
#include "core/protocol_parser.hpp"
#include "network/tcp_server.hpp"
#include "storage/flush_worker.hpp"
#include "storage/redis_storage.hpp"

namespace {

std::string get_env_string(const char* name, std::string_view fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return std::string(fallback);
    }

    return value;
}

int get_env_int(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    char* end = nullptr;
    const long parsed_value = std::strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        return fallback;
    }

    return static_cast<int>(parsed_value);
}

}  // namespace

int main() {
    std::cout << "[Server]: Initializing HydraMetrics engine..." << std::endl;

    hydra::core::ProtocolParser parser;
    hydra::core::MetricAggregator aggregator;
    hydra::storage::RedisStorage storage;

    const std::string redis_host = get_env_string("REDIS_HOST", "127.0.0.1");
    const int redis_port = get_env_int("REDIS_PORT", 6337);
    if (!storage.connect(redis_host, redis_port)) {
        std::cerr << "[Fatal]: Failed to connect to Redis storage at [" << redis_host << ':' << redis_port << "]."
                  << std::endl;
        return 1;
    }

    hydra::storage::FlushWorker flush_worker(aggregator, storage);
    flush_worker.start(std::chrono::milliseconds(1000));

    hydra::network::TcpServer server(parser, aggregator);
    const std::string server_address = get_env_string("SERVER_ADDRESS", "0.0.0.0");
    const int server_port = get_env_int("SERVER_PORT", 8080);
    const std::error_code start_error = server.start(server_address, static_cast<uint16_t>(server_port));
    if (start_error) {
        flush_worker.stop();
        return 1;
    }

    boost::asio::io_context signal_context;
    boost::asio::signal_set signals(signal_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        server.stop();
        flush_worker.stop();
        std::cout << "[Server]: Stopping background worker and performing final memory-to-db flush..." << std::endl;
        auto final_snapshot = aggregator.extract_snapshot();
        storage.flush_snapshot(final_snapshot);
        std::cout << "[Server]: Final flush completed. Goodbye." << std::endl;
        signal_context.stop();
    });

    signal_context.run();

    server.stop();
    flush_worker.stop();
    return 0;
}
