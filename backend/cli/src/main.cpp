#include "watch.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

void print_usage() {
    std::cerr
        << "usage: auracle <command> [options]\n"
        << "\n"
        << "commands:\n"
        << "  inventory watch   stream inventory events from the daemon\n"
        << "\n"
        << "inventory watch options:\n"
        << "  --server <addr>   daemon address (default: 127.0.0.1:50051)\n"
        << "  --initial         request initial snapshot before live events\n"
        << "  --format <fmt>    output format: pretty, json, prototext (default: pretty)\n"
        << "  --no-candidates   exclude candidate events\n"
        << "  --no-units        exclude unit events\n"
        << "  --verbose         show extra detail\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 2;
    }

    const std::string_view cmd{argv[1]};
    const std::string_view sub{argv[2]};

    if (cmd != "inventory" || sub != "watch") {
        std::cerr << "unknown command: " << cmd << " " << sub << "\n";
        print_usage();
        return 2;
    }

    auracle::cli::WatchOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--initial") {
            opts.initial = true;
        } else if (arg == "--format" && i + 1 < argc) {
            const std::string_view fmt{argv[++i]};
            if (fmt == "pretty") {
                opts.format = auracle::cli::OutputFormat::Pretty;
            } else if (fmt == "json") {
                opts.format = auracle::cli::OutputFormat::Json;
            } else if (fmt == "prototext") {
                opts.format = auracle::cli::OutputFormat::Prototext;
            } else {
                std::cerr << "unknown format: " << fmt << "\n";
                return 2;
            }
        } else if (arg == "--no-candidates") {
            opts.include_candidates = false;
        } else if (arg == "--no-units") {
            opts.include_units = false;
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_watch(opts);
}
