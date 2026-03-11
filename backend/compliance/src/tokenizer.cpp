#include <compliance/tokenizer.hpp>

#include <compliance/parser.hpp>

#include <array>
#include <charconv>
#include <cctype>
#include <string>

namespace auracle::compliance {
namespace {

[[nodiscard]] bool is_identifier_char(char ch) noexcept {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

[[nodiscard]] bool is_identifier_start(char ch) noexcept {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

[[nodiscard]] TokenKind keyword_kind(std::string_view text) {
    static constexpr std::array keywords{
        std::pair{"TEST", TokenKind::kw_test},
        std::pair{"TITLE", TokenKind::kw_title},
        std::pair{"WHEN", TokenKind::kw_when},
        std::pair{"THEN", TokenKind::kw_then},
        std::pair{"MESSAGE", TokenKind::kw_message},
        std::pair{"REFERENCE", TokenKind::kw_reference},
        std::pair{"FAIL", TokenKind::kw_fail},
        std::pair{"WARN", TokenKind::kw_warn},
        std::pair{"INFO", TokenKind::kw_info},
        std::pair{"AND", TokenKind::kw_and},
        std::pair{"OR", TokenKind::kw_or},
        std::pair{"NOT", TokenKind::kw_not},
        std::pair{"EA", TokenKind::kw_ea},
        std::pair{"HAS", TokenKind::kw_has},
        std::pair{"LACKS", TokenKind::kw_lacks},
        std::pair{"EQUALS", TokenKind::kw_equals},
        std::pair{"NOT_EQUALS", TokenKind::kw_not_equals},
    };

    for (const auto& [keyword, kind] : keywords) {
        if (text == keyword) {
            return kind;
        }
    }

    return TokenKind::identifier;
}

[[nodiscard]] std::string format_message(std::string_view message, const SourceLocation& location) {
    return std::to_string(location.line) + ":" + std::to_string(location.column) + ": " +
           std::string(message);
}

[[noreturn]] void throw_parse_error(std::string_view message, const SourceLocation& location) {
    throw ParseError(format_message(message, location), location.line, location.column);
}

} // namespace

Tokenizer::Tokenizer(std::string_view input)
    : input_(input) {}

Token Tokenizer::next() {
    skip_whitespace();

    const SourceLocation location = current_location();
    if (at_end()) {
        return Token{
            .kind = TokenKind::end_of_file,
            .location = location,
        };
    }

    const char ch = peek();
    if (ch == '.') {
        advance();
        return Token{
            .kind = TokenKind::dot,
            .location = location,
            .text = ".",
        };
    }
    if (ch == '(') {
        advance();
        return Token{
            .kind = TokenKind::left_paren,
            .location = location,
            .text = "(",
        };
    }
    if (ch == ')') {
        advance();
        return Token{
            .kind = TokenKind::right_paren,
            .location = location,
            .text = ")",
        };
    }
    if (ch == '"') {
        return string_literal();
    }
    if (ch == '0' && peek_next() == 'x') {
        return uuid_literal();
    }
    if (is_identifier_start(ch)) {
        return identifier_or_keyword();
    }

    throw_parse_error("Unexpected character", location);
}

bool Tokenizer::at_end() const noexcept {
    return offset_ >= input_.size();
}

char Tokenizer::peek() const noexcept {
    return at_end() ? '\0' : input_[offset_];
}

char Tokenizer::peek_next() const noexcept {
    return (offset_ + 1) < input_.size() ? input_[offset_ + 1] : '\0';
}

char Tokenizer::advance() noexcept {
    const char ch = input_[offset_++];
    if (ch == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return ch;
}

void Tokenizer::skip_whitespace() {
    while (!at_end()) {
        const char ch = peek();
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            advance();
            continue;
        }
        break;
    }
}

SourceLocation Tokenizer::current_location() const noexcept {
    return SourceLocation{.line = line_, .column = column_};
}

Token Tokenizer::identifier_or_keyword() {
    const SourceLocation location = current_location();
    const std::size_t start = offset_;

    while (!at_end() && is_identifier_char(peek())) {
        advance();
    }

    const std::string text(input_.substr(start, offset_ - start));
    return Token{
        .kind = keyword_kind(text),
        .location = location,
        .text = text,
    };
}

Token Tokenizer::string_literal() {
    const SourceLocation location = current_location();
    advance();

    std::string value;
    while (!at_end()) {
        const char ch = advance();
        if (ch == '"') {
            return Token{
                .kind = TokenKind::string_literal,
                .location = location,
                .text = std::move(value),
            };
        }

        if (ch == '\\') {
            if (at_end()) {
                throw_parse_error("Unterminated string literal", location);
            }

            const char escaped = advance();
            switch (escaped) {
                case '"':
                    value.push_back('"');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                default:
                    throw_parse_error("Unsupported escape sequence", location);
            }
            continue;
        }

        if (ch == '\n') {
            throw_parse_error("Unterminated string literal", location);
        }

        value.push_back(ch);
    }

    throw_parse_error("Unterminated string literal", location);
}

Token Tokenizer::uuid_literal() {
    const SourceLocation location = current_location();
    const std::size_t start = offset_;

    advance();
    advance();

    std::size_t digits = 0;
    while (!at_end() && std::isxdigit(static_cast<unsigned char>(peek())) != 0) {
        ++digits;
        advance();
    }

    if (digits != 4) {
        throw_parse_error("UUID literal must be exactly 4 hex digits", location);
    }

    if (!at_end()) {
        const char next = peek();
        if (std::isalnum(static_cast<unsigned char>(next)) != 0 || next == '_') {
            throw_parse_error("UUID literal must end after 4 hex digits", location);
        }
    }

    const std::string text(input_.substr(start, offset_ - start));
    std::uint16_t value = 0;
    const char* begin = text.data() + 2;
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value, 16);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw_parse_error("Invalid UUID literal", location);
    }

    return Token{
        .kind = TokenKind::uuid_literal,
        .location = location,
        .text = text,
        .uuid = value,
    };
}

} // namespace auracle::compliance
