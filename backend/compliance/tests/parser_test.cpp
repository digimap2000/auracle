#include <gtest/gtest.h>

#include <compliance/ast.hpp>
#include <compliance/parser.hpp>

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

TEST(ParserTest, ParsesValidRule) {
    Rule rule = parse_rule(kRuleText);

    EXPECT_EQ(rule.id, "AURACAST_SRC_001");
    ASSERT_TRUE(rule.title.has_value());
    EXPECT_EQ(*rule.title, "Public Broadcast Announcement required for Auracast Source");
    EXPECT_EQ(rule.verdict, Verdict::fail);
    EXPECT_EQ(rule.message,
              "Source advertises as a Broadcast Source but is missing the Public Broadcast "
              "Announcement - not Auracast compliant");
    ASSERT_TRUE(rule.reference.has_value());
    EXPECT_EQ(*rule.reference, "PBP Section 4.1");

    const auto* expression = std::get_if<BinaryExpression>(&rule.when.node);
    ASSERT_NE(expression, nullptr);
    EXPECT_EQ(expression->op, BooleanOp::and_);

    const auto* left = std::get_if<Predicate>(&expression->left->node);
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->scope, Scope::ea);
    EXPECT_EQ(left->path.kind, FactPathKind::service_data);
    ASSERT_TRUE(left->path.key.has_value());
    EXPECT_EQ(*left->path.key, 0x1852);
    EXPECT_EQ(left->op, PredicateOp::has);
    EXPECT_FALSE(left->value.has_value());

    const auto* right = std::get_if<Predicate>(&expression->right->node);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->scope, Scope::ea);
    EXPECT_EQ(right->path.kind, FactPathKind::service_data);
    ASSERT_TRUE(right->path.key.has_value());
    EXPECT_EQ(*right->path.key, 0x1856);
    EXPECT_EQ(right->op, PredicateOp::lacks);
    EXPECT_FALSE(right->value.has_value());
}

TEST(ParserTest, RejectsMalformedSyntax) {
    constexpr std::string_view kBadRule = R"(TEST AURACAST_SRC_001
WHEN EA HAS service_data
THEN FAIL
MESSAGE "bad rule")";

    EXPECT_THROW(
        {
            try {
                (void)parse_rule(kBadRule);
            } catch (const ParseError& error) {
                EXPECT_GT(error.line(), 0U);
                EXPECT_GT(error.column(), 0U);
                throw;
            }
        },
        ParseError);
}

TEST(ParserTest, ParsesAppearancePredicateRule) {
    constexpr std::string_view kAppearanceRule = R"(TEST AURACAST_SRC_002
WHEN EA HAS service_data.broadcast_audio_announcement
 AND EA.ad.appearance NOT_EQUALS 0x0041
THEN FAIL
MESSAGE "Appearance is not set to Generic Audio Source"
REFERENCE "Bluetooth Assigned Numbers Section 6.1")";

    Rule rule = parse_rule(kAppearanceRule);

    const auto* expression = std::get_if<BinaryExpression>(&rule.when.node);
    ASSERT_NE(expression, nullptr);
    const auto* right = std::get_if<Predicate>(&expression->right->node);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->path.kind, FactPathKind::ad_appearance);
    EXPECT_FALSE(right->path.key.has_value());
    EXPECT_EQ(right->op, PredicateOp::not_equals);
    ASSERT_TRUE(right->value.has_value());
    EXPECT_EQ(*right->value, 0x0041);
}

} // namespace
} // namespace auracle::compliance
