#include <gtest/gtest.h>

#include <compliance/evaluator.hpp>
#include <compliance/parser.hpp>

#include <optional>
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

constexpr std::string_view kAppearanceRuleText = R"(TEST AURACAST_SRC_002
TITLE "Appearance should identify an Auracast Source as Generic Audio Source"
WHEN EA HAS service_data.broadcast_audio_announcement
 AND EA.ad.appearance NOT_EQUALS 0x0041
THEN FAIL
MESSAGE "Source appearance is not set to Generic Audio Source (0x0041) for a Naim audio streamer"
REFERENCE "Bluetooth Assigned Numbers Section 6.1")";

TEST(EvaluatorTest, TriggersFailWhen1852PresentAnd1856Absent) {
    Rule rule = parse_rule(kRuleText);

    ComplianceFacts facts;
    facts.ea.service_data_uuids.insert(0x1852);

    const std::optional<Finding> finding = evaluate_rule(rule, facts);
    ASSERT_TRUE(finding.has_value());
    EXPECT_EQ(finding->test_id, "AURACAST_SRC_001");
    EXPECT_EQ(finding->verdict, Verdict::fail);
    EXPECT_EQ(finding->message,
              "Source advertises as a Broadcast Source but is missing the Public Broadcast "
              "Announcement - not Auracast compliant");
    ASSERT_TRUE(finding->reference.has_value());
    EXPECT_EQ(*finding->reference, "PBP Section 4.1");
}

TEST(EvaluatorTest, DoesNotTriggerWhenBothArePresent) {
    Rule rule = parse_rule(kRuleText);

    ComplianceFacts facts;
    facts.ea.service_data_uuids.insert(0x1852);
    facts.ea.service_data_uuids.insert(0x1856);

    const std::optional<Finding> finding = evaluate_rule(rule, facts);
    EXPECT_FALSE(finding.has_value());
}

TEST(EvaluatorTest, DoesNotTriggerWhen1852Absent) {
    Rule rule = parse_rule(kRuleText);

    ComplianceFacts facts;
    facts.ea.service_data_uuids.insert(0x1856);

    const std::optional<Finding> finding = evaluate_rule(rule, facts);
    EXPECT_FALSE(finding.has_value());
}

TEST(EvaluatorTest, TriggersFailWhenAppearanceIsMissingOrWrong) {
    Rule rule = parse_rule(kAppearanceRuleText);

    ComplianceFacts facts;
    facts.ea.service_data_uuids.insert(0x1852);
    facts.ea.appearance = 0x0042;

    const std::optional<Finding> finding = evaluate_rule(rule, facts);
    ASSERT_TRUE(finding.has_value());
    EXPECT_EQ(finding->test_id, "AURACAST_SRC_002");
    EXPECT_EQ(finding->verdict, Verdict::fail);
}

TEST(EvaluatorTest, DoesNotTriggerWhenAppearanceMatchesExpectedValue) {
    Rule rule = parse_rule(kAppearanceRuleText);

    ComplianceFacts facts;
    facts.ea.service_data_uuids.insert(0x1852);
    facts.ea.appearance = 0x0041;

    const std::optional<Finding> finding = evaluate_rule(rule, facts);
    EXPECT_FALSE(finding.has_value());
}

} // namespace
} // namespace auracle::compliance
