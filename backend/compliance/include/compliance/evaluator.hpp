#pragma once

#include <compliance/ast.hpp>
#include <compliance/facts.hpp>

#include <optional>
#include <string>

namespace auracle::compliance {

struct Finding {
    std::string test_id;
    Verdict verdict;
    std::string message;
    std::optional<std::string> reference;
};

[[nodiscard]] bool evaluate_predicate(const Predicate& predicate, const ComplianceFacts& facts);
[[nodiscard]] bool evaluate_expression(const Expression& expression, const ComplianceFacts& facts);
[[nodiscard]] std::optional<Finding> evaluate_rule(const Rule& rule, const ComplianceFacts& facts);

} // namespace auracle::compliance
