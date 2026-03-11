#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace auracle::compliance {

enum class Verdict {
    fail,
    warn,
    info,
};

enum class Scope {
    ea,
};

enum class FactPathKind {
    service_data,
    ad_appearance,
};

struct FactPath {
    FactPathKind kind;
    std::optional<std::uint16_t> key;
};

enum class PredicateOp {
    has,
    lacks,
    equals,
    not_equals,
};

struct Predicate {
    Scope scope;
    FactPath path;
    PredicateOp op;
    std::optional<std::uint16_t> value;
};

struct Expression;

struct NotExpression {
    std::unique_ptr<Expression> operand;
};

enum class BooleanOp {
    and_,
    or_,
};

struct BinaryExpression {
    BooleanOp op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct Expression {
    std::variant<Predicate, NotExpression, BinaryExpression> node;
};

struct Rule {
    std::string id;
    std::optional<std::string> title;
    Expression when;
    Verdict verdict;
    std::string message;
    std::optional<std::string> reference;
};

} // namespace auracle::compliance
