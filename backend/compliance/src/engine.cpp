#include <compliance/engine.hpp>

#include <compliance/normalizer.hpp>

namespace auracle::compliance {

std::vector<Finding> evaluate_rules(std::span<const Rule> rules, const ComplianceFacts& facts) {
    std::vector<Finding> findings;
    findings.reserve(rules.size());

    for (const Rule& rule : rules) {
        if (auto finding = evaluate_rule(rule, facts); finding.has_value()) {
            findings.push_back(std::move(*finding));
        }
    }

    return findings;
}

std::vector<Finding> evaluate_rules(
    std::span<const Rule> rules,
    const observation::BleAdvertisement& advertisement) {
    return evaluate_rules(rules, normalize_ea_facts(advertisement));
}

} // namespace auracle::compliance
