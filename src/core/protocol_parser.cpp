#include "core/protocol_parser.hpp"

#include <bit>
#include <string_view>

namespace hydra::core {
namespace {

constexpr bool is_valid_metric_type(uint8_t type) noexcept {
    return type == static_cast<uint8_t>(MetricType::Counter) || type == static_cast<uint8_t>(MetricType::Gauge);
}

uint64_t read_big_endian_u64(const uint8_t* bytes) noexcept {
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

std::error_code bad_message_error() noexcept {
    return std::make_error_code(std::errc::bad_message);
}

}  // namespace

std::pair<bool, size_t> ProtocolParser::parse_header(std::span<const uint8_t> header_bytes) noexcept {
    if (header_bytes.size() < ProtocolConstants::HEADER_SIZE) {
        return {false, 0};
    }

    if (header_bytes[0] != ProtocolConstants::MAGIC_BYTE || !is_valid_metric_type(header_bytes[1])) {
        return {false, 0};
    }

    [[maybe_unused]] const uint64_t timestamp = read_big_endian_u64(header_bytes.data() + 2);
    const size_t name_len = header_bytes[10];

    return {true, ProtocolConstants::HEADER_SIZE + name_len + sizeof(double)};
}

std::error_code ProtocolParser::deserialize(std::span<const uint8_t> packet_bytes, MetricPacket& out_packet) noexcept {
    const auto [is_valid_header, expected_size] = parse_header(packet_bytes);
    if (!is_valid_header || packet_bytes.size() != expected_size) {
        return bad_message_error();
    }

    const size_t name_len = packet_bytes[10];
    const size_t name_offset = ProtocolConstants::HEADER_SIZE;
    const size_t value_offset = name_offset + name_len;

    out_packet.type = static_cast<MetricType>(packet_bytes[1]);
    out_packet.timestamp = read_big_endian_u64(packet_bytes.data() + 2);

    const auto* name_data = reinterpret_cast<const char*>(packet_bytes.data() + name_offset);
    const std::string_view name_view{name_data, name_len};
    out_packet.name.assign(name_view);

    const uint64_t value_bits = read_big_endian_u64(packet_bytes.data() + value_offset);
    out_packet.value = std::bit_cast<double>(value_bits);

    return {};
}

}  // namespace hydra::core
