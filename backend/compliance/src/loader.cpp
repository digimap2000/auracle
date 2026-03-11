#include <compliance/loader.hpp>

#include <compliance/parser.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace auracle::compliance {

Rule load_rule_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open rule file: " + path.string());
    }

    std::string text{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    return parse_rule(text);
}

std::vector<Rule> load_rules_from_directory(
    const std::filesystem::path& path,
    const std::filesystem::path& extension) {
    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }

    std::ranges::sort(files);

    std::vector<Rule> rules;
    rules.reserve(files.size());
    for (const auto& file : files) {
        rules.push_back(load_rule_file(file));
    }

    return rules;
}

} // namespace auracle::compliance
