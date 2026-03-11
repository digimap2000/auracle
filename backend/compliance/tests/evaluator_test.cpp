#include <gtest/gtest.h>

#include <compliance/evaluator.hpp>
#include <compliance/parser.hpp>

#include <optional>
#include <string_view>

namespace auracle::compliance {
namespace {

constexpr std::string_view kRuleText = R"(TEST AURACAST_SRC_001
TITLE "Public Broadcast Announcement required for Auracast Source"
WHEN EA HAS_SERVICE_DATA 0x1852
 AND EA LACKS_SERVICE_DATA 0x1856
THEN FAIL
MESSAGE "Source advertises as a Broadcast Source but is missing the Public Broadcast Announcement - not Auracast compliant"
REFERENCE "PBP Section 4.1")";

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

} // namespace
} // namespace auracle::compliance
