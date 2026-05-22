#pragma once

#include <cstdint>
#include <string>
#include <system_error>

namespace hydra::network {

class INetworkServer {
public:
    virtual ~INetworkServer() = default;

    /**
     * @brief Конфигурация, инициализация и запуск асинхронного TCP-сервера.
     *        Метод аллоцирует пул потоков Asio и запускает бесконечный цикл
     *        обработки сетевых событий (Reactor паттерн) через корутины C++20.
     *
     * @param address IP-адрес для прослушивания (например, "0.0.0.0" или "127.0.0.1").
     * @param port Сетевой порт (например, 8080).
     * @return std::error_code Системный код ошибки. Возвращает default (успех), если сервер запустился.
     */
    virtual std::error_code start(const std::string& address, uint16_t port) = 0;

    /**
     * @brief Грациозная остановка сервера (Graceful Shutdown).
     *        Останавливает io_context, дожидается завершения обработки текущих
     *        активных сессий и плавно останавливает рабочие потоки.
     */
    virtual void stop() noexcept = 0;
};

}  // namespace hydra::network
