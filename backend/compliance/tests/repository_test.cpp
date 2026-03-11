#include <gtest/gtest.h>

#include <compliance/repository.hpp>

#include <filesystem>
#include <fstream>

namespace auracle::compliance {
namespace {

TEST(RepositoryTest, ResolvesRulesForSuite) {
    const auto base = std::filesystem::temp_directory_path() / "auracle_compliance_repository_test";
    const auto rules_dir = base / "rules";
    const auto suites_dir = base / "suites";
    std::filesystem::create_directories(rules_dir);
    std::filesystem::create_directories(suites_dir);

    {
        std::ofstream output(rules_dir / "rule.rule");
        output << "TEST AURACAST_SRC_001\n"
               << "WHEN EA HAS service_data.0x1852\n"
               << "THEN INFO\n"
               << "MESSAGE \"Example rule\"\n";
    }

    {
        std::ofstream output(suites_dir / "suite.suite");
        output << "SUITE AURACAST_SOURCE_BASELINE\n"
               << "TITLE \"Auracast Source Baseline\"\n"
               << "RULE AURACAST_SRC_001\n";
    }

    Repository repository(rules_dir, suites_dir);
    const auto rules = repository.rules_for_suite("AURACAST_SOURCE_BASELINE");

    ASSERT_EQ(rules.size(), 1U);
    EXPECT_EQ(rules.front().get().rule.id, "AURACAST_SRC_001");

    std::filesystem::remove_all(base);
}

} // namespace
} // namespace auracle::compliance
