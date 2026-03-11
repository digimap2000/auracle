#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace auracle::compliance {

struct Suite {
    std::string id;
    std::optional<std::string> title;
    std::vector<std::string> rule_ids;
};

struct LoadedSuite {
    Suite suite;
    std::filesystem::path path;
};

[[nodiscard]] LoadedSuite load_suite_file(const std::filesystem::path& path);
[[nodiscard]] std::vector<LoadedSuite> load_suites_from_directory(
    const std::filesystem::path& path,
    const std::filesystem::path& extension = ".suite");

} // namespace auracle::compliance
