#pragma once

#include "options.hpp"

#include <string>

namespace auracle::cli {

struct ComplianceOptions {
    std::string server = "127.0.0.1:50051";
    std::string unit_id;
    std::string rule_id;
    std::string suite_id;
    std::string rule_path;
    std::string rules_dir;
    std::string raw_data_hex;
    std::string raw_scan_response_hex;
    OutputFormat format = OutputFormat::Pretty;
    bool verbose = false;
};

[[nodiscard]] int run_compliance_eval(const ComplianceOptions& opts);
[[nodiscard]] int run_compliance_list_rules(const ComplianceOptions& opts);
[[nodiscard]] int run_compliance_list_suites(const ComplianceOptions& opts);
[[nodiscard]] int run_compliance_run_rule(const ComplianceOptions& opts);
[[nodiscard]] int run_compliance_run_suite(const ComplianceOptions& opts);

} // namespace auracle::cli
