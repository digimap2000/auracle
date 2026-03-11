#pragma once

#include <compliance/loader.hpp>
#include <compliance/suite_loader.hpp>

#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

namespace auracle::compliance {

class Repository {
public:
    Repository();
    Repository(std::filesystem::path rules_dir, std::filesystem::path suites_dir);

    [[nodiscard]] const std::vector<LoadedRule>& rules() const noexcept { return rules_; }
    [[nodiscard]] const std::vector<LoadedSuite>& suites() const noexcept { return suites_; }

    [[nodiscard]] const LoadedRule* find_rule(std::string_view id) const noexcept;
    [[nodiscard]] const LoadedSuite* find_suite(std::string_view id) const noexcept;
    [[nodiscard]] std::vector<std::reference_wrapper<const LoadedRule>>
    rules_for_suite(std::string_view suite_id) const;

private:
    std::filesystem::path rules_dir_;
    std::filesystem::path suites_dir_;
    std::vector<LoadedRule> rules_;
    std::vector<LoadedSuite> suites_;
};

} // namespace auracle::compliance
