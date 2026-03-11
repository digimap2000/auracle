#pragma once

#include <compliance/ast.hpp>

#include <filesystem>
#include <vector>

namespace auracle::compliance {

[[nodiscard]] Rule load_rule_file(const std::filesystem::path& path);
[[nodiscard]] std::vector<Rule> load_rules_from_directory(
    const std::filesystem::path& path,
    const std::filesystem::path& extension = ".rule");

} // namespace auracle::compliance
