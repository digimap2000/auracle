#include <compliance/evaluator.hpp>

#include <type_traits>

namespace auracle::compliance {
namespace {

bool evaluate_service_data_predicate(const Predicate& predicate, const ComplianceFacts& facts) {
    const auto& service_data_uuids = facts.ea.service_data_uuids;
    const std::uint16_t uuid = *predicate.path.key;

    switch (predicate.op) {
        case PredicateOp::has:
            return service_data_uuids.contains(uuid);
        case PredicateOp::lacks:
            return !service_data_uuids.contains(uuid);
        case PredicateOp::equals:
        case PredicateOp::not_equals:
            return false;
    }

    return false;
}

bool evaluate_appearance_predicate(const Predicate& predicate, const ComplianceFacts& facts) {
    const auto appearance = facts.ea.appearance;

    switch (predicate.op) {
        case PredicateOp::has:
            return appearance.has_value();
        case PredicateOp::lacks:
            return !appearance.has_value();
        case PredicateOp::equals:
            return appearance.has_value() && *appearance == *predicate.value;
        case PredicateOp::not_equals:
            return !appearance.has_value() || *appearance != *predicate.value;
    }

    return false;
}

} // namespace

bool evaluate_predicate(const Predicate& predicate, const ComplianceFacts& facts) {
    switch (predicate.path.kind) {
        case FactPathKind::service_data:
            return evaluate_service_data_predicate(predicate, facts);
        case FactPathKind::ad_appearance:
            return evaluate_appearance_predicate(predicate, facts);
    }

    return false;
}

bool evaluate_expression(const Expression& expression, const ComplianceFacts& facts) {
    return std::visit(
        [&facts](const auto& node) -> bool {
            using node_type = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<node_type, Predicate>) {
                return evaluate_predicate(node, facts);
            } else if constexpr (std::is_same_v<node_type, NotExpression>) {
                return !evaluate_expression(*node.operand, facts);
            } else if constexpr (std::is_same_v<node_type, BinaryExpression>) {
                const bool left = evaluate_expression(*node.left, facts);
                const bool right = evaluate_expression(*node.right, facts);
                return node.op == BooleanOp::and_ ? (left && right) : (left || right);
            } else {
                static_assert(!sizeof(node_type), "Unhandled expression node type");
            }
        },
        expression.node);
}

std::optional<Finding> evaluate_rule(const Rule& rule, const ComplianceFacts& facts) {
    if (!evaluate_expression(rule.when, facts)) {
        return std::nullopt;
    }

    return Finding{
        .test_id = rule.id,
        .verdict = rule.verdict,
        .message = rule.message,
        .reference = rule.reference,
    };
}

} // namespace auracle::compliance
