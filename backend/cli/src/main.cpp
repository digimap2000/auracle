#include "list.hpp"
#include "watch.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

using auracle::cli::OutputFormat;

bool parse_format(std::string_view fmt, OutputFormat& out) {
    if (fmt == "pretty")    { out = OutputFormat::Pretty;    return true; }
    if (fmt == "json")      { out = OutputFormat::Json;      return true; }
    if (fmt == "prototext") { out = OutputFormat::Prototext; return true; }
    return false;
}

void print_usage() {
    std::cerr
        << "usage: auracle <command> [options]\n"
        << "\n"
        << "commands:\n"
        << "  inventory list    show current candidates and units\n"
        << "  inventory watch   stream inventory events from the daemon\n"
        << "\n"
        << "common options:\n"
        << "  --server <addr>   daemon address (default: 127.0.0.1:50051)\n"
        << "  --format <fmt>    output format: pretty, json, prototext (default: pretty)\n"
        << "  --no-candidates   exclude candidates\n"
        << "  --no-units        exclude units\n"
        << "  --verbose         show extra detail\n"
        << "\n"
        << "inventory list options:\n"
        << "  --include-gone    include departed candidates\n"
        << "  --include-offline include offline units\n"
        << "\n"
        << "inventory watch options:\n"
        << "  --initial         request initial snapshot before live events\n";
}

int run_list_main(int argc, char* argv[]) {
    auracle::cli::ListOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--no-candidates") {
            opts.include_candidates = false;
        } else if (arg == "--no-units") {
            opts.include_units = false;
        } else if (arg == "--include-gone") {
            opts.include_gone = true;
        } else if (arg == "--include-offline") {
            opts.include_offline = true;
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_list(opts);
}

int run_watch_main(int argc, char* argv[]) {
    auracle::cli::WatchOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--initial") {
            opts.initial = true;
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
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

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 2;
    }

    const std::string_view cmd{argv[1]};
    const std::string_view sub{argv[2]};

    if (cmd != "inventory") {
        std::cerr << "unknown command: " << cmd << "\n";
        print_usage();
        return 2;
    }

    if (sub == "list")  return run_list_main(argc, argv);
    if (sub == "watch") return run_watch_main(argc, argv);

    std::cerr << "unknown subcommand: " << sub << "\n";
    print_usage();
    return 2;
}
