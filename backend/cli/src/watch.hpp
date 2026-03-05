#pragma once

#include <string>

namespace auracle::cli {

enum class OutputFormat {
    Pretty,
    Json,
    Prototext,
};

struct WatchOptions {
    std::string server = "127.0.0.1:50051";
    bool initial = false;
    OutputFormat format = OutputFormat::Pretty;
    bool include_candidates = true;
    bool include_units = true;
    bool verbose = false;
};

[[nodiscard]] int run_watch(const WatchOptions& opts);

} // namespace auracle::cli
