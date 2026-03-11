#include "compliance.hpp"
#include "list.hpp"
#include "scan.hpp"
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
        << "  compliance eval         evaluate local compliance rules against raw advertising bytes\n"
        << "  compliance list-rules   list daemon-hosted compliance rules\n"
        << "  compliance list-suites  list daemon-hosted compliance suites\n"
        << "  compliance run-rule     run one daemon-hosted rule against a unit\n"
        << "  compliance run-suite    run one daemon-hosted suite against a unit\n"
        << "  inventory list    show current candidates and units\n"
        << "  inventory watch   stream inventory events from the daemon\n"
        << "  scan list <id>    show retained BLE scan results for a unit\n"
        << "  scan watch <id>   stream BLE advertisements from a unit\n"
        << "  scan decode       decode raw advertising payloads via the daemon\n"
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
        << "  --initial         request initial snapshot before live events\n"
        << "\n"
        << "scan list options:\n"
        << "  --verbose         show extra detail\n"
        << "\n"
        << "scan watch options:\n"
        << "  --initial         include the retained snapshot before live events\n"
        << "  --no-duplicates   only report first advertisement per device\n"
        << "\n"
        << "scan decode options:\n"
        << "  --raw <hex>       raw advertising payload bytes\n"
        << "  --scan-response <hex>\n"
        << "                   raw scan response payload bytes\n"
        << "\n"
        << "compliance eval options:\n"
        << "  --rule <path>     evaluate one rule file\n"
        << "  --rules-dir <dir> evaluate all .rule files in a directory\n"
        << "  --server <addr>   daemon address for list/run commands (default: 127.0.0.1:50051)\n"
        << "  --raw <hex>       raw advertising payload bytes\n"
        << "  --scan-response <hex>\n"
        << "                   raw scan response payload bytes (ignored for EA-only rules)\n"
        << "\n"
        << "compliance run-rule usage:\n"
        << "  auracle compliance run-rule <scanner-unit-id> <observed-device-id> <rule-id> [--server <addr>] [--format <fmt>]\n"
        << "\n"
        << "compliance run-suite usage:\n"
        << "  auracle compliance run-suite <scanner-unit-id> <observed-device-id> <suite-id> [--server <addr>] [--format <fmt>]\n";
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

int run_scan_watch_main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "usage: auracle scan watch <unit-id> [options]\n";
        return 2;
    }

    auracle::cli::ScanOptions opts;
    opts.unit_id = argv[3];

    for (int i = 4; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--initial") {
            opts.include_initial_snapshot = true;
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--no-duplicates") {
            opts.allow_duplicates = false;
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_scan(opts);
}

int run_scan_list_main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "usage: auracle scan list <unit-id> [options]\n";
        return 2;
    }

    auracle::cli::ScanOptions opts;
    opts.unit_id = argv[3];

    for (int i = 4; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_scan_list(opts);
}

int run_scan_decode_main(int argc, char* argv[]) {
    auracle::cli::ScanOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--raw" && i + 1 < argc) {
            opts.raw_data_hex = argv[++i];
        } else if (arg == "--scan-response" && i + 1 < argc) {
            opts.raw_scan_response_hex = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    if (opts.raw_data_hex.empty() && opts.raw_scan_response_hex.empty()) {
        std::cerr << "usage: auracle scan decode --raw <hex> [--scan-response <hex>] [options]\n";
        return 2;
    }

    return auracle::cli::run_scan_decode(opts);
}

int run_compliance_eval_main(int argc, char* argv[]) {
    auracle::cli::ComplianceOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--rule" && i + 1 < argc) {
            opts.rule_path = argv[++i];
        } else if (arg == "--rules-dir" && i + 1 < argc) {
            opts.rules_dir = argv[++i];
        } else if (arg == "--raw" && i + 1 < argc) {
            opts.raw_data_hex = argv[++i];
        } else if (arg == "--scan-response" && i + 1 < argc) {
            opts.raw_scan_response_hex = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_compliance_eval(opts);
}

int run_compliance_list_rules_main(int argc, char* argv[]) {
    auracle::cli::ComplianceOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_compliance_list_rules(opts);
}

int run_compliance_list_suites_main(int argc, char* argv[]) {
    auracle::cli::ComplianceOptions opts;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_compliance_list_suites(opts);
}

int run_compliance_run_rule_main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "usage: auracle compliance run-rule <scanner-unit-id> <observed-device-id> <rule-id> [options]\n";
        return 2;
    }

    auracle::cli::ComplianceOptions opts;
    opts.scanner_unit_id = argv[3];
    opts.observed_device_id = argv[4];
    opts.rule_id = argv[5];

    for (int i = 6; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_compliance_run_rule(opts);
}

int run_compliance_run_suite_main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "usage: auracle compliance run-suite <scanner-unit-id> <observed-device-id> <suite-id> [options]\n";
        return 2;
    }

    auracle::cli::ComplianceOptions opts;
    opts.scanner_unit_id = argv[3];
    opts.observed_device_id = argv[4];
    opts.suite_id = argv[5];

    for (int i = 6; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--server" && i + 1 < argc) {
            opts.server = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            if (!parse_format(argv[++i], opts.format)) {
                std::cerr << "unknown format: " << argv[i] << "\n";
                return 2;
            }
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    return auracle::cli::run_compliance_run_suite(opts);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 2;
    }

    const std::string_view cmd{argv[1]};
    const std::string_view sub{argv[2]};

    if (cmd == "inventory") {
        if (sub == "list")  return run_list_main(argc, argv);
        if (sub == "watch") return run_watch_main(argc, argv);
    } else if (cmd == "compliance") {
        if (sub == "eval") return run_compliance_eval_main(argc, argv);
        if (sub == "list-rules") return run_compliance_list_rules_main(argc, argv);
        if (sub == "list-suites") return run_compliance_list_suites_main(argc, argv);
        if (sub == "run-rule") return run_compliance_run_rule_main(argc, argv);
        if (sub == "run-suite") return run_compliance_run_suite_main(argc, argv);
    } else if (cmd == "scan") {
        if (sub == "list")  return run_scan_list_main(argc, argv);
        if (sub == "watch") return run_scan_watch_main(argc, argv);
        if (sub == "decode") return run_scan_decode_main(argc, argv);
    }

    std::cerr << "unknown command: " << cmd << " " << sub << "\n";
    print_usage();
    return 2;
}
