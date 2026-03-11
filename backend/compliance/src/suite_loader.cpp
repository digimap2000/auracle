#include <compliance/suite_loader.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <stdexcept>

namespace auracle::compliance {
namespace {

[[nodiscard]] bool is_identifier_start(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') || ch == '_';
}

[[nodiscard]] bool is_identifier_char(char ch) noexcept {
    return is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

[[nodiscard]] std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
        --end;
    }

    return std::string{text.substr(begin, end - begin)};
}

[[nodiscard]] std::string parse_identifier(std::string_view text, std::string_view field) {
    const std::string value = trim(text);
    if (value.empty()) {
        throw std::runtime_error(std::string(field) + " requires an identifier");
    }
    if (!is_identifier_start(value.front())) {
        throw std::runtime_error(std::string(field) + " identifier must start with A-Z or _");
    }
    if (!std::ranges::all_of(value, is_identifier_char)) {
        throw std::runtime_error(std::string(field) + " identifier contains invalid characters");
    }
    return value;
}

[[nodiscard]] std::string parse_string(std::string_view text, std::string_view field) {
    const std::string value = trim(text);
    if (value.size() < 2U || value.front() != '"' || value.back() != '"') {
        throw std::runtime_error(std::string(field) + " requires a quoted string");
    }
    return value.substr(1U, value.size() - 2U);
}

} // namespace

LoadedSuite load_suite_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open suite file: " + path.string());
    }

    Suite suite;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }

        const auto first_space = trimmed.find(' ');
        const std::string keyword = trimmed.substr(0, first_space);
        const std::string_view remainder = first_space == std::string::npos
            ? std::string_view{}
            : std::string_view{trimmed}.substr(first_space + 1U);

        if (keyword == "SUITE") {
            if (!suite.id.empty()) {
                throw std::runtime_error("Duplicate SUITE clause in " + path.string());
            }
            suite.id = parse_identifier(remainder, "SUITE");
        } else if (keyword == "TITLE") {
            if (suite.title.has_value()) {
                throw std::runtime_error("Duplicate TITLE clause in " + path.string());
            }
            suite.title = parse_string(remainder, "TITLE");
        } else if (keyword == "RULE") {
            suite.rule_ids.push_back(parse_identifier(remainder, "RULE"));
        } else {
            throw std::runtime_error(
                "Unknown suite keyword '" + keyword + "' in " + path.string() + ":" +
                std::to_string(line_number));
        }
    }

    if (suite.id.empty()) {
        throw std::runtime_error("Missing SUITE clause in " + path.string());
    }
    if (suite.rule_ids.empty()) {
        throw std::runtime_error("Suite " + suite.id + " contains no RULE entries");
    }

    return LoadedSuite{
        .suite = std::move(suite),
        .path = path,
    };
}

std::vector<LoadedSuite> load_suites_from_directory(
    const std::filesystem::path& path,
    const std::filesystem::path& extension) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Suite directory does not exist: " + path.string());
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }

    std::ranges::sort(files);

    std::vector<LoadedSuite> suites;
    suites.reserve(files.size());
    for (const auto& file : files) {
        suites.push_back(load_suite_file(file));
    }
    return suites;
}

} // namespace auracle::compliance
