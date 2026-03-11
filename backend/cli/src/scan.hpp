#pragma once

#include "options.hpp"

#include <string>

namespace auracle::cli {

struct ScanOptions {
    std::string server = "127.0.0.1:50051";
    std::string unit_id;
    bool allow_duplicates = true;
    bool include_initial_snapshot = false;
    OutputFormat format = OutputFormat::Pretty;
    bool verbose = false;
};

[[nodiscard]] int run_scan(const ScanOptions& opts);
[[nodiscard]] int run_scan_list(const ScanOptions& opts);

} // namespace auracle::cli
