#include "core/protocol_parser.hpp"

#include <gtest/gtest.h>

namespace hydra::core {
namespace {

TEST(ProtocolParserTest, ParseValidHeader) {
    ProtocolParser parser;
    const uint8_t header[] = {
        0x48, 0x01,                   // Magic, Counter
        0x00, 0x00, 0x00, 0x00,       // Timestamp
        0x00, 0x00, 0x00, 0x01, 0x05  // Timestamp, Name length
    };

    const auto [is_valid, expected_size] = parser.parse_header(header);

    EXPECT_TRUE(is_valid);
    EXPECT_EQ(expected_size, 24U);
}

TEST(ProtocolParserTest, ParseInvalidMagic) {
    ProtocolParser parser;
    const uint8_t header[] = {
        0x00, 0x01,                   // Invalid magic, Counter
        0x00, 0x00, 0x00, 0x00,       // Timestamp
        0x00, 0x00, 0x00, 0x01, 0x05  // Timestamp, Name length
    };

    const auto [is_valid, expected_size] = parser.parse_header(header);

    EXPECT_FALSE(is_valid);
    EXPECT_EQ(expected_size, 0U);
}

TEST(ProtocolParserTest, ParseInvalidType) {
    ProtocolParser parser;
    const uint8_t header[] = {
        0x48, 0x03,                   // Magic, Invalid type
        0x00, 0x00, 0x00, 0x00,       // Timestamp
        0x00, 0x00, 0x00, 0x01, 0x05  // Timestamp, Name length
    };

    const auto [is_valid, expected_size] = parser.parse_header(header);

    EXPECT_FALSE(is_valid);
    EXPECT_EQ(expected_size, 0U);
}

TEST(ProtocolParserTest, DeserializeValidPacket) {
    ProtocolParser parser;
    MetricPacket packet;
    const std::vector<uint8_t> packet_bytes = {
        0x48, 0x01,                                     // Magic, Counter
        0x00, 0x01, 0x02, 0x03,                         // Timestamp
        0x04, 0x05, 0x06, 0x07, 0x04,                   // Timestamp, Name length
        0x74, 0x65, 0x73, 0x74,                         // "test"
        0x40, 0x45, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00  // 42.5, big-endian IEEE-754
    };

    const std::error_code error = parser.deserialize(packet_bytes, packet);

    EXPECT_EQ(error, std::error_code{});
    EXPECT_EQ(packet.type, MetricType::Counter);
    EXPECT_EQ(packet.timestamp, 0x0001020304050607ULL);
    EXPECT_EQ(packet.name, "test");
    EXPECT_DOUBLE_EQ(packet.value, 42.5);
}

TEST(ProtocolParserTest, DeserializeCorruptedPacket) {
    ProtocolParser parser;
    MetricPacket packet;
    const std::vector<uint8_t> packet_bytes = {
        0x48, 0x01,                    // Magic, Counter
        0x00, 0x00, 0x00, 0x00,        // Timestamp
        0x00, 0x00, 0x00, 0x01, 0x05,  // Timestamp, Name length
        0x74, 0x65                     // Truncated metric name
    };

    const std::error_code error = parser.deserialize(packet_bytes, packet);

    EXPECT_EQ(error, std::make_error_code(std::errc::bad_message));
}

}  // namespace
}  // namespace hydra::core
