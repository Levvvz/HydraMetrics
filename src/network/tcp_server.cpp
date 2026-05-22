#include "network/tcp_server.hpp"

#include <algorithm>
#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <thread>
#include <vector>

#include "core/types.hpp"

namespace hydra::network {

namespace asio = boost::asio;
using boost::system::error_code;

namespace {

std::error_code to_std_error_code(const error_code& error) {
    if (!error) {
        return {};
    }

    return {error.value(), std::system_category()};
}

}  // namespace

TcpServer::TcpServer(core::IProtocolParser& parser, core::IMetricAggregator& aggregator) noexcept
    : parser_(parser), aggregator_(aggregator) {
}

TcpServer::~TcpServer() {
    stop();
}

std::error_code TcpServer::start(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);

    if (io_context_) {
        return std::make_error_code(std::errc::already_connected);
    }

    const auto thread_count = static_cast<int>(std::max(1U, std::thread::hardware_concurrency()));
    auto io_context = std::make_unique<asio::io_context>(thread_count);
    auto thread_pool = std::make_unique<asio::thread_pool>(thread_count);
    auto work_guard = std::make_unique<WorkGuard>(io_context->get_executor());
    auto acceptor = std::make_unique<Tcp::acceptor>(*io_context);

    error_code error;
    const auto listen_address = asio::ip::make_address(address, error);
    if (error) {
        return to_std_error_code(error);
    }

    const Tcp::endpoint endpoint(listen_address, port);
    acceptor->open(endpoint.protocol(), error);
    if (error) {
        return to_std_error_code(error);
    }

    acceptor->set_option(asio::socket_base::reuse_address(true), error);
    if (error) {
        return to_std_error_code(error);
    }

    acceptor->bind(endpoint, error);
    if (error) {
        return to_std_error_code(error);
    }

    acceptor->listen(asio::socket_base::max_listen_connections, error);
    if (error) {
        return to_std_error_code(error);
    }

    io_context_ = std::move(io_context);
    thread_pool_ = std::move(thread_pool);
    work_guard_ = std::move(work_guard);
    acceptor_ = std::move(acceptor);

    asio::co_spawn(*io_context_, listener_loop(), asio::detached);

    for (int index = 0; index < thread_count; ++index) {
        asio::post(*thread_pool_, [io_context = io_context_.get()] { io_context->run(); });
    }

    return {};
}

void TcpServer::stop() noexcept {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    reset_runtime();
}

asio::awaitable<void> TcpServer::listener_loop() {
    while (acceptor_ && acceptor_->is_open()) {
        error_code error;
        Tcp::socket socket = co_await acceptor_->async_accept(asio::redirect_error(asio::use_awaitable, error));

        if (error == asio::error::operation_aborted) {
            break;
        }
        if (error) {
            continue;
        }

        asio::co_spawn(socket.get_executor(), session_loop(std::move(socket)), asio::detached);
    }
}

asio::awaitable<void> TcpServer::session_loop(Tcp::socket socket) {
    std::array<uint8_t, core::ProtocolConstants::HEADER_SIZE> header{};

    for (;;) {
        error_code error;
        co_await asio::async_read(socket, asio::buffer(header), asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            break;
        }

        const auto [is_valid_header, expected_size] = parser_.parse_header(header);
        if (!is_valid_header || expected_size < core::ProtocolConstants::HEADER_SIZE) {
            break;
        }

        std::vector<uint8_t> packet(expected_size);
        std::copy(header.begin(), header.end(), packet.begin());

        const size_t body_size = expected_size - core::ProtocolConstants::HEADER_SIZE;
        co_await asio::async_read(socket, asio::buffer(packet.data() + core::ProtocolConstants::HEADER_SIZE, body_size),
                                  asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            break;
        }

        core::MetricPacket metric;
        const std::error_code deserialize_error = parser_.deserialize(packet, metric);
        if (deserialize_error) {
            break;
        }

        aggregator_.update_metric(metric);
    }
}

void TcpServer::reset_runtime() noexcept {
    try {
        error_code ignored_error;
        if (acceptor_) {
            acceptor_->close(ignored_error);
        }
        if (work_guard_) {
            work_guard_.reset();
        }
        if (io_context_) {
            io_context_->stop();
        }
        if (thread_pool_) {
            thread_pool_->join();
        }

        acceptor_.reset();
        work_guard_.reset();
        thread_pool_.reset();
        io_context_.reset();
    } catch (...) {
    }
}

}  // namespace hydra::network
