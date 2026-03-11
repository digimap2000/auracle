#include <gtest/gtest.h>

#include <compliance/engine.hpp>
#include <compliance/parser.hpp>

#include <observation/types.hpp>

#include <string_view>

namespace auracle::compliance {
namespace {

constexpr std::string_view kRuleText = R"(TEST AURACAST_SRC_001
TITLE "Public Broadcast Announcement required for Auracast Source"
WHEN EA HAS service_data.broadcast_audio_announcement
 AND EA LACKS service_data.public_broadcast_announcement
THEN FAIL
MESSAGE "Source advertises as a Broadcast Source but is missing the Public Broadcast Announcement - not Auracast compliant"
REFERENCE "PBP Section 4.1")";

TEST(EngineTest, EvaluatesRuleAgainstBleAdvertisement) {
    const Rule rule = parse_rule(kRuleText);

    observation::BleAdvertisement advertisement;
    advertisement.raw_data = {
        0x05, 0x16, 0x52, 0x18, 0x01, 0x02,
    };

    const auto findings = evaluate_rules(std::span{&rule, 1}, advertisement);
    ASSERT_EQ(findings.size(), 1U);
    EXPECT_EQ(findings.front().test_id, "AURACAST_SRC_001");
    EXPECT_EQ(findings.front().verdict, Verdict::fail);
}

} // namespace
} // namespace auracle::compliance
