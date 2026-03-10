#pragma once

#include "options.hpp"

#include <string>

namespace auracle::cli {

struct ListOptions {
    std::string server = "127.0.0.1:50051";
    OutputFormat format = OutputFormat::Pretty;
    bool include_candidates = true;
    bool include_units = true;
    bool include_gone = false;
    bool include_offline = false;
    bool verbose = false;
};

[[nodiscard]] int run_list(const ListOptions& opts);

} // namespace auracle::cli
