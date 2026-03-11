#include <compliance/parser.hpp>

#include <compliance/tokenizer.hpp>

#include <array>
#include <cctype>
#include <memory>
#include <optional>
#include <utility>

namespace auracle::compliance {
namespace {

[[nodiscard]] Expression make_expression(Predicate predicate) {
    return Expression{.node = std::move(predicate)};
}

[[nodiscard]] Expression make_expression(NotExpression expression) {
    return Expression{.node = std::move(expression)};
}

[[nodiscard]] Expression make_expression(BinaryExpression expression) {
    return Expression{.node = std::move(expression)};
}

[[nodiscard]] std::unique_ptr<Expression> make_expression_ptr(Expression expression) {
    return std::make_unique<Expression>(std::move(expression));
}

[[nodiscard]] const char* token_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::end_of_file:
            return "end of file";
        case TokenKind::identifier:
            return "identifier";
        case TokenKind::string_literal:
            return "string literal";
        case TokenKind::uuid_literal:
            return "UUID literal";
        case TokenKind::dot:
            return ".";
        case TokenKind::left_paren:
            return "(";
        case TokenKind::right_paren:
            return ")";
        case TokenKind::kw_test:
            return "TEST";
        case TokenKind::kw_title:
            return "TITLE";
        case TokenKind::kw_when:
            return "WHEN";
        case TokenKind::kw_then:
            return "THEN";
        case TokenKind::kw_message:
            return "MESSAGE";
        case TokenKind::kw_reference:
            return "REFERENCE";
        case TokenKind::kw_fail:
            return "FAIL";
        case TokenKind::kw_warn:
            return "WARN";
        case TokenKind::kw_info:
            return "INFO";
        case TokenKind::kw_and:
            return "AND";
        case TokenKind::kw_or:
            return "OR";
        case TokenKind::kw_not:
            return "NOT";
        case TokenKind::kw_ea:
            return "EA";
        case TokenKind::kw_has:
            return "HAS";
        case TokenKind::kw_lacks:
            return "LACKS";
        case TokenKind::kw_equals:
            return "EQUALS";
        case TokenKind::kw_not_equals:
            return "NOT_EQUALS";
    }

    return "token";
}

[[nodiscard]] bool is_rule_identifier(std::string_view text) noexcept {
    if (text.empty()) {
        return false;
    }

    const char first = text.front();
    if (!((first >= 'A' && first <= 'Z') || first == '_')) {
        return false;
    }

    for (const char ch : text) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
            continue;
        }
        return false;
    }

    return true;
}

[[nodiscard]] std::optional<std::uint16_t> service_data_alias(std::string_view name) {
    static constexpr std::array aliases{
        std::pair{"broadcast_audio_announcement", static_cast<std::uint16_t>(0x1852)},
        std::pair{"public_broadcast_announcement", static_cast<std::uint16_t>(0x1856)},
    };

    for (const auto& [alias, uuid] : aliases) {
        if (name == alias) {
            return uuid;
        }
    }

    return std::nullopt;
}

class Parser {
public:
    explicit Parser(std::string_view input)
        : tokenizer_(input), current_(tokenizer_.next()) {}

    [[nodiscard]] Rule parse_rule() {
        (void)consume(TokenKind::kw_test, "Expected TEST clause");
        Token id = consume_identifier("Expected test identifier");
        if (!is_rule_identifier(id.text)) {
            throw ParseError(
                "Test identifiers must use uppercase letters, digits, and underscores",
                id.location.line,
                id.location.column);
        }

        std::optional<std::string> title;
        if (match(TokenKind::kw_title)) {
            title = consume(TokenKind::string_literal, "Expected TITLE string").text;
        }

        (void)consume(TokenKind::kw_when, "Expected WHEN clause");
        Expression when = parse_expression();

        (void)consume(TokenKind::kw_then, "Expected THEN clause");
        Verdict verdict = parse_verdict();

        (void)consume(TokenKind::kw_message, "Expected MESSAGE clause");
        std::string message = consume(TokenKind::string_literal, "Expected MESSAGE string").text;

        std::optional<std::string> reference;
        if (match(TokenKind::kw_reference)) {
            reference = consume(TokenKind::string_literal, "Expected REFERENCE string").text;
        }

        (void)consume(TokenKind::end_of_file, "Unexpected trailing input");

        return Rule{
            .id = std::move(id.text),
            .title = std::move(title),
            .when = std::move(when),
            .verdict = verdict,
            .message = std::move(message),
            .reference = std::move(reference),
        };
    }

private:
    [[nodiscard]] Expression parse_expression() {
        return parse_or_expression();
    }

    [[nodiscard]] Expression parse_or_expression() {
        Expression left = parse_and_expression();

        while (match(TokenKind::kw_or)) {
            Expression right = parse_and_expression();
            left = make_expression(BinaryExpression{
                .op = BooleanOp::or_,
                .left = make_expression_ptr(std::move(left)),
                .right = make_expression_ptr(std::move(right)),
            });
        }

        return left;
    }

    [[nodiscard]] Expression parse_and_expression() {
        Expression left = parse_unary_expression();

        while (match(TokenKind::kw_and)) {
            Expression right = parse_unary_expression();
            left = make_expression(BinaryExpression{
                .op = BooleanOp::and_,
                .left = make_expression_ptr(std::move(left)),
                .right = make_expression_ptr(std::move(right)),
            });
        }

        return left;
    }

    [[nodiscard]] Expression parse_unary_expression() {
        if (match(TokenKind::kw_not)) {
            return make_expression(NotExpression{
                .operand = make_expression_ptr(parse_unary_expression()),
            });
        }

        return parse_primary_expression();
    }

    [[nodiscard]] Expression parse_primary_expression() {
        if (match(TokenKind::left_paren)) {
            Expression expression = parse_expression();
            (void)consume(TokenKind::right_paren, "Expected ')' after expression");
            return expression;
        }

        return make_expression(parse_predicate());
    }

    [[nodiscard]] Predicate parse_predicate() {
        (void)consume(TokenKind::kw_ea, "Expected scope");

        if (match(TokenKind::kw_has)) {
            return parse_membership_predicate(PredicateOp::has);
        }
        if (match(TokenKind::kw_lacks)) {
            return parse_membership_predicate(PredicateOp::lacks);
        }
        if (match(TokenKind::dot)) {
            FactPath path = parse_scoped_fact_path();
            PredicateOp op = parse_comparison_op();
            const Token value = consume(TokenKind::uuid_literal, "Expected 16-bit UUID literal");
            validate_predicate(path, op, value.location);
            return Predicate{
                .scope = Scope::ea,
                .path = std::move(path),
                .op = op,
                .value = value.uuid,
            };
        }

        throw error_here("Expected predicate operator");
    }

    [[nodiscard]] Predicate parse_membership_predicate(PredicateOp op) {
        FactPath path = parse_fact_path();
        validate_predicate(path, op, current_.location);
        return Predicate{
            .scope = Scope::ea,
            .path = std::move(path),
            .op = op,
            .value = std::nullopt,
        };
    }

    [[nodiscard]] FactPath parse_fact_path() {
        const Token root = consume(TokenKind::identifier, "Expected fact path");
        if (root.text == "service_data") {
            (void)consume(TokenKind::dot, "Expected '.' after service_data");
            return FactPath{
                .kind = FactPathKind::service_data,
                .key = parse_service_data_key(),
            };
        }
        if (root.text == "ad") {
            (void)consume(TokenKind::dot, "Expected '.' after ad");
            const Token leaf = consume(TokenKind::identifier, "Expected advertising property");
            if (leaf.text != "appearance") {
                throw ParseError(
                    "Unsupported advertising property '" + leaf.text + "'",
                    leaf.location.line,
                    leaf.location.column);
            }
            return FactPath{
                .kind = FactPathKind::ad_appearance,
                .key = std::nullopt,
            };
        }

        throw ParseError(
            "Unsupported fact path root '" + root.text + "'",
            root.location.line,
            root.location.column);
    }

    [[nodiscard]] FactPath parse_scoped_fact_path() {
        return parse_fact_path();
    }

    [[nodiscard]] std::uint16_t parse_service_data_key() {
        if (current_.kind == TokenKind::uuid_literal) {
            const Token token = current_;
            advance();
            return token.uuid;
        }

        const Token alias = consume(TokenKind::identifier, "Expected service_data key");
        if (const auto resolved = service_data_alias(alias.text)) {
            return *resolved;
        }

        throw ParseError(
            "Unknown service_data alias '" + alias.text + "'",
            alias.location.line,
            alias.location.column);
    }

    [[nodiscard]] PredicateOp parse_comparison_op() {
        if (match(TokenKind::kw_equals)) {
            return PredicateOp::equals;
        }
        if (match(TokenKind::kw_not_equals)) {
            return PredicateOp::not_equals;
        }
        throw error_here("Expected comparison operator");
    }

    void validate_predicate(
        const FactPath& path,
        PredicateOp op,
        const SourceLocation& location) const {
        switch (path.kind) {
            case FactPathKind::service_data:
                if (op == PredicateOp::has || op == PredicateOp::lacks) {
                    return;
                }
                throw ParseError(
                    "service_data paths only support HAS or LACKS",
                    location.line,
                    location.column);
            case FactPathKind::ad_appearance:
                return;
        }
    }

    [[nodiscard]] Verdict parse_verdict() {
        if (match(TokenKind::kw_fail)) {
            return Verdict::fail;
        }
        if (match(TokenKind::kw_warn)) {
            return Verdict::warn;
        }
        if (match(TokenKind::kw_info)) {
            return Verdict::info;
        }
        throw error_here("Expected verdict");
    }

    [[nodiscard]] bool match(TokenKind kind) {
        if (current_.kind != kind) {
            return false;
        }
        advance();
        return true;
    }

    Token consume(TokenKind kind, std::string_view message) {
        if (current_.kind != kind) {
            throw error_expected(kind, message);
        }
        Token token = current_;
        advance();
        return token;
    }

    Token consume_identifier(std::string_view message) {
        if (current_.kind != TokenKind::identifier) {
            throw error_here(message);
        }
        Token token = current_;
        advance();
        return token;
    }

    void advance() {
        current_ = tokenizer_.next();
    }

    [[nodiscard]] ParseError error_here(std::string_view message) const {
        return ParseError(std::string(message), current_.location.line, current_.location.column);
    }

    [[nodiscard]] ParseError error_expected(TokenKind kind, std::string_view message) const {
        std::string detail(message);
        detail.append(": expected ");
        detail.append(token_name(kind));
        detail.append(", found ");
        detail.append(token_name(current_.kind));
        if (!current_.text.empty()) {
            detail.append(" '");
            detail.append(current_.text);
            detail.push_back('\'');
        }
        return ParseError(std::move(detail), current_.location.line, current_.location.column);
    }

    Tokenizer tokenizer_;
    Token current_;
};

} // namespace

ParseError::ParseError(std::string message, std::size_t line, std::size_t column)
    : std::runtime_error(std::move(message)),
      line_(line),
      column_(column) {}

Rule parse_rule(std::string_view text) {
    Parser parser(text);
    return parser.parse_rule();
}

} // namespace auracle::compliance
