#include <gtest/gtest.h>

#include <compliance/loader.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>

namespace auracle::compliance {
namespace {

constexpr std::string_view kRuleText = R"(TEST AURACAST_SRC_001
WHEN EA HAS service_data.0x1852
THEN INFO
MESSAGE "Example rule")";

TEST(LoaderTest, LoadsRuleFile) {
    const auto path =
        std::filesystem::temp_directory_path() / "auracle_compliance_loader_test.rule";

    {
        std::ofstream output(path);
        ASSERT_TRUE(output.is_open());
        output << kRuleText;
    }

    const LoadedRule loaded = load_rule_file(path);
    EXPECT_EQ(loaded.rule.id, "AURACAST_SRC_001");
    EXPECT_EQ(loaded.rule.verdict, Verdict::info);
    EXPECT_EQ(loaded.rule.message, "Example rule");

    std::filesystem::remove(path);
}

} // namespace
} // namespace auracle::compliance
