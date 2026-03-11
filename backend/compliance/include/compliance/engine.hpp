#pragma once

#include <compliance/evaluator.hpp>
#include <compliance/facts.hpp>

#include <observation/types.hpp>

#include <span>
#include <vector>

namespace auracle::compliance {

[[nodiscard]] std::vector<Finding> evaluate_rules(
    std::span<const Rule> rules,
    const ComplianceFacts& facts);

[[nodiscard]] std::vector<Finding> evaluate_rules(
    std::span<const Rule> rules,
    const observation::BleAdvertisement& advertisement);

} // namespace auracle::compliance
