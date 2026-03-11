#include <gtest/gtest.h>

#include <compliance/normalizer.hpp>

#include <observation/types.hpp>

namespace auracle::compliance {
namespace {

TEST(NormalizerTest, ExtractsServiceDataUuidFromEaPayload) {
    const std::vector<std::uint8_t> raw_data = {
        0x05, 0x16, 0x52, 0x18, 0x01, 0x02,
        0x03, 0x03, 0x52, 0x18,
    };

    const ComplianceFacts facts = normalize_ea_facts(raw_data);
    EXPECT_TRUE(facts.ea.service_data_uuids.contains(0x1852));
    EXPECT_FALSE(facts.ea.service_data_uuids.contains(0x1856));
    EXPECT_FALSE(facts.ea.appearance.has_value());
}

TEST(NormalizerTest, IgnoresScanResponseWhenNormalizingAdvertisement) {
    observation::BleAdvertisement advertisement;
    advertisement.raw_data = {
        0x05, 0x16, 0x52, 0x18, 0x01, 0x02,
    };
    advertisement.raw_scan_response = {
        0x05, 0x16, 0x56, 0x18, 0xAA, 0xBB,
    };

    const ComplianceFacts facts = normalize_ea_facts(advertisement);
    EXPECT_TRUE(facts.ea.service_data_uuids.contains(0x1852));
    EXPECT_FALSE(facts.ea.service_data_uuids.contains(0x1856));
}

TEST(NormalizerTest, ExtractsAppearanceFromEaPayload) {
    const std::vector<std::uint8_t> raw_data = {
        0x03, 0x19, 0x41, 0x00,
        0x05, 0x16, 0x52, 0x18, 0x01, 0x02,
    };

    const ComplianceFacts facts = normalize_ea_facts(raw_data);
    ASSERT_TRUE(facts.ea.appearance.has_value());
    EXPECT_EQ(*facts.ea.appearance, 0x0041);
    EXPECT_TRUE(facts.ea.service_data_uuids.contains(0x1852));
}

} // namespace
} // namespace auracle::compliance
