#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace hydra::core {

// Тип метрики согласно спецификации протокола
enum class MetricType : uint8_t {
    Counter = 0x01,  // Инкрементальный счетчик
    Gauge = 0x02     // Текущее фиксированное значение
};

// Константы протокола
struct ProtocolConstants {
    static constexpr uint8_t MAGIC_BYTE = 0x48;    // Символ 'H'
    static constexpr size_t MIN_PACKET_SIZE = 20;  // 1 + 1 + 8 + 1 + 1 + 8 (при длине имени 1 байт)
    static constexpr size_t HEADER_SIZE = 11;      // Magic(1) + Type(1) + Timestamp(8) + NameLen(1)
};

// Структура десериализованной метрики, готовой к передаче в агрегатор
struct MetricPacket {
    MetricType type;
    uint64_t timestamp;
    std::string name;
    double value;
};

}  // namespace hydra::core
