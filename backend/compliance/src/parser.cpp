#include <compliance/parser.hpp>

#include <compliance/tokenizer.hpp>

#include <memory>
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
        case TokenKind::kw_has_service_data:
            return "HAS_SERVICE_DATA";
        case TokenKind::kw_lacks_service_data:
            return "LACKS_SERVICE_DATA";
    }

    return "token";
}

class Parser {
public:
    explicit Parser(std::string_view input)
        : tokenizer_(input), current_(tokenizer_.next()) {}

    [[nodiscard]] Rule parse_rule() {
        (void)consume(TokenKind::kw_test, "Expected TEST clause");
        const std::string id = consume_identifier("Expected test identifier").text;

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
            .id = id,
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
        const Token scope_token = consume(TokenKind::kw_ea, "Expected scope");
        (void)scope_token;

        PredicateKind kind{};
        if (match(TokenKind::kw_has_service_data)) {
            kind = PredicateKind::has_service_data;
        } else if (match(TokenKind::kw_lacks_service_data)) {
            kind = PredicateKind::lacks_service_data;
        } else {
            throw error_here("Expected predicate operator");
        }

        const Token uuid = consume(TokenKind::uuid_literal, "Expected 16-bit UUID literal");

        return Predicate{
            .scope = Scope::ea,
            .kind = kind,
            .uuid = uuid.uuid,
        };
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
