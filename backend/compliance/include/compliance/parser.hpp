#pragma once

#include <compliance/ast.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace auracle::compliance {

class ParseError : public std::runtime_error {
public:
    ParseError(std::string message, std::size_t line, std::size_t column);

    [[nodiscard]] std::size_t line() const noexcept { return line_; }
    [[nodiscard]] std::size_t column() const noexcept { return column_; }

private:
    std::size_t line_;
    std::size_t column_;
};

[[nodiscard]] Rule parse_rule(std::string_view text);

} // namespace auracle::compliance
