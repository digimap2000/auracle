#include <compliance/repository.hpp>

#include <stdexcept>

namespace auracle::compliance {
namespace {

[[nodiscard]] std::filesystem::path default_rules_dir() {
    return std::filesystem::path{AURACLE_COMPLIANCE_RULE_DIR};
}

[[nodiscard]] std::filesystem::path default_suites_dir() {
    return std::filesystem::path{AURACLE_COMPLIANCE_SUITE_DIR};
}

} // namespace

Repository::Repository()
    : Repository(default_rules_dir(), default_suites_dir()) {}

Repository::Repository(std::filesystem::path rules_dir, std::filesystem::path suites_dir)
    : rules_dir_(std::move(rules_dir)),
      suites_dir_(std::move(suites_dir)),
      rules_(load_rules_from_directory(rules_dir_)),
      suites_(load_suites_from_directory(suites_dir_)) {
    for (const LoadedSuite& loaded_suite : suites_) {
        for (const std::string& rule_id : loaded_suite.suite.rule_ids) {
            if (find_rule(rule_id) == nullptr) {
                throw std::runtime_error(
                    "Suite " + loaded_suite.suite.id + " references unknown rule " + rule_id);
            }
        }
    }
}

const LoadedRule* Repository::find_rule(std::string_view id) const noexcept {
    for (const LoadedRule& loaded_rule : rules_) {
        if (loaded_rule.rule.id == id) {
            return &loaded_rule;
        }
    }
    return nullptr;
}

const LoadedSuite* Repository::find_suite(std::string_view id) const noexcept {
    for (const LoadedSuite& loaded_suite : suites_) {
        if (loaded_suite.suite.id == id) {
            return &loaded_suite;
        }
    }
    return nullptr;
}

std::vector<std::reference_wrapper<const LoadedRule>>
Repository::rules_for_suite(std::string_view suite_id) const {
    const LoadedSuite* loaded_suite = find_suite(suite_id);
    if (loaded_suite == nullptr) {
        throw std::runtime_error("Unknown compliance suite: " + std::string(suite_id));
    }

    std::vector<std::reference_wrapper<const LoadedRule>> rules;
    rules.reserve(loaded_suite->suite.rule_ids.size());

    for (const std::string& rule_id : loaded_suite->suite.rule_ids) {
        const LoadedRule* loaded_rule = find_rule(rule_id);
        if (loaded_rule == nullptr) {
            throw std::runtime_error(
                "Suite " + loaded_suite->suite.id + " references unknown rule " + rule_id);
        }
        rules.push_back(std::cref(*loaded_rule));
    }

    return rules;
}

} // namespace auracle::compliance
