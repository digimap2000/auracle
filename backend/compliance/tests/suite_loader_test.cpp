#include <gtest/gtest.h>

#include <compliance/suite_loader.hpp>

#include <filesystem>
#include <fstream>

namespace auracle::compliance {
namespace {

TEST(SuiteLoaderTest, LoadsSuiteFile) {
    const auto path =
        std::filesystem::temp_directory_path() / "auracle_compliance_suite_loader_test.suite";

    {
        std::ofstream output(path);
        ASSERT_TRUE(output.is_open());
        output << "SUITE AURACAST_SOURCE_BASELINE\n"
               << "TITLE \"Auracast Source Baseline\"\n"
               << "RULE AURACAST_SRC_001\n";
    }

    const LoadedSuite loaded = load_suite_file(path);
    EXPECT_EQ(loaded.suite.id, "AURACAST_SOURCE_BASELINE");
    ASSERT_TRUE(loaded.suite.title.has_value());
    EXPECT_EQ(*loaded.suite.title, "Auracast Source Baseline");
    ASSERT_EQ(loaded.suite.rule_ids.size(), 1U);
    EXPECT_EQ(loaded.suite.rule_ids.front(), "AURACAST_SRC_001");

    std::filesystem::remove(path);
}

} // namespace
} // namespace auracle::compliance
