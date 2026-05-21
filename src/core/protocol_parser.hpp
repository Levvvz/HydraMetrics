#pragma once

#include "types.hpp"
#include <span;>
#include <system_error>
#include <utility>

namespace hydra::core {

class IProtocolParser {
public:
    virtual ~IProtocolParser() = default;

    /**
     * @brief Предварительный анализ заголовка пакета.
     * @param header_bytes Срез байт, содержащий как минимум заголовок (11 байт).
     * @return pair<bool, size_t>: 
     *         - bool: true, если заголовок валиден (совпадает Magic Byte и тип).
     *         - size_t: ожидаемый ПОЛНЫЙ размер всего пакета (включая имя и value).
     */
    virtual std::pair<bool, size_t> parse_header(std::span<const uint8_t> header_bytes) noexcept = 0;

    /**
     * @brief Полная десериализация пакета.
     * @param packet_bytes Полный срез байт пакета, вычитанный из сокета.
     * @param out_packet Ссылка на структуру, куда будет записан результат.
     * @return std::error_code Системный код ошибки. Возвращает default (успех), если пакет распарсен.
     */
    virtual std::error_code deserialize(std::span<const uint8_t> packet_bytes, MetricPacket& out_packet) noexcept = 0;
};

} // namespace hydra::core
