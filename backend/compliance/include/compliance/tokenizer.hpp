#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace auracle::compliance {

enum class TokenKind {
    end_of_file,
    identifier,
    string_literal,
    uuid_literal,
    dot,
    left_paren,
    right_paren,
    kw_test,
    kw_title,
    kw_when,
    kw_then,
    kw_message,
    kw_reference,
    kw_fail,
    kw_warn,
    kw_info,
    kw_and,
    kw_or,
    kw_not,
    kw_ea,
    kw_has,
    kw_lacks,
    kw_equals,
    kw_not_equals,
};

struct SourceLocation {
    std::size_t line{1};
    std::size_t column{1};
};

struct Token {
    TokenKind kind{TokenKind::end_of_file};
    SourceLocation location{};
    std::string text;
    std::uint16_t uuid{0};
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input);

    [[nodiscard]] Token next();

private:
    [[nodiscard]] bool at_end() const noexcept;
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peek_next() const noexcept;
    char advance() noexcept;
    void skip_whitespace();
    [[nodiscard]] SourceLocation current_location() const noexcept;
    [[nodiscard]] Token identifier_or_keyword();
    [[nodiscard]] Token string_literal();
    [[nodiscard]] Token uuid_literal();

    std::string_view input_;
    std::size_t offset_{0};
    std::size_t line_{1};
    std::size_t column_{1};
};

} // namespace auracle::compliance
