#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "core/aggregator_interface.hpp"
#include "core/protocol_parser.hpp"
#include "network/server_interface.hpp"

namespace hydra::network {

class TcpServer final : public INetworkServer {
public:
    TcpServer(core::IProtocolParser& parser, core::IMetricAggregator& aggregator) noexcept;

    ~TcpServer() override;

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    std::error_code start(const std::string& address, uint16_t port) override;

    void stop() noexcept override;

private:
    using Tcp = boost::asio::ip::tcp;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    boost::asio::awaitable<void> listener_loop();

    boost::asio::awaitable<void> session_loop(Tcp::socket socket);

    void reset_runtime() noexcept;

    core::IProtocolParser& parser_;
    core::IMetricAggregator& aggregator_;
    std::mutex lifecycle_mutex_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::thread_pool> thread_pool_;
    std::unique_ptr<WorkGuard> work_guard_;
    std::unique_ptr<Tcp::acceptor> acceptor_;
};

}  // namespace hydra::network
