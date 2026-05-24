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

TEST(ProtocolParserTest, DeserializeEmptyNameBoundary) {
    ProtocolParser parser;
    MetricPacket packet;
    const std::vector<uint8_t> packet_bytes = {
        0x48, 0x01,                                     // Magic, Counter
        0x00, 0x00, 0x00, 0x00,                         // Timestamp
        0x00, 0x00, 0x00, 0x01, 0x00,                   // Timestamp, Empty name length
        0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 1.0, big-endian IEEE-754
    };

    const std::error_code error = parser.deserialize(packet_bytes, packet);

    EXPECT_EQ(error, std::error_code{});
    EXPECT_EQ(packet.type, MetricType::Counter);
    EXPECT_EQ(packet.timestamp, 1ULL);
    EXPECT_TRUE(packet.name.empty());
    EXPECT_DOUBLE_EQ(packet.value, 1.0);
}

TEST(ProtocolParserTest, DeserializeMaxNameBoundary) {
    ProtocolParser parser;
    MetricPacket packet;
    std::vector<uint8_t> packet_bytes = {
        0x48, 0x02,                   // Magic, Gauge
        0x00, 0x00, 0x00, 0x00,       // Timestamp
        0x00, 0x00, 0x00, 0x02, 0xff  // Timestamp, Max name length
    };
    const std::string expected_name(255, 'A');
    packet_bytes.insert(packet_bytes.end(), expected_name.begin(), expected_name.end());
    packet_bytes.insert(packet_bytes.end(), {
                                                0x40,
                                                0x09,
                                                0x21,
                                                0xfb,
                                                0x54,
                                                0x44,
                                                0x2d,
                                                0x18,
                                            });

    const std::error_code error = parser.deserialize(packet_bytes, packet);

    EXPECT_EQ(error, std::error_code{});
    EXPECT_EQ(packet.type, MetricType::Gauge);
    EXPECT_EQ(packet.timestamp, 2ULL);
    EXPECT_EQ(packet.name.size(), 255U);
    EXPECT_EQ(packet.name, expected_name);
    EXPECT_DOUBLE_EQ(packet.value, 3.141592653589793);
}

}  // namespace
}  // namespace hydra::core
