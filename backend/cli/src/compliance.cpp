#include "compliance.hpp"

#include <compliance/engine.hpp>
#include <compliance/loader.hpp>

#include <auracle/compliance/v1/compliance.grpc.pb.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>

namespace auracle::cli {

namespace compliance_dsl = ::auracle::compliance;
namespace compliance_proto = ::auracle::compliance::v1;

namespace {

std::vector<std::uint8_t> hex_to_bytes(std::string_view hex) {
    std::vector<std::uint8_t> out;
    if ((hex.size() % 2U) != 0U) {
        return out;
    }

    out.reserve(hex.size() / 2U);
    for (std::size_t i = 0; i < hex.size(); i += 2U) {
        auto parse_nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };

        const auto hi = parse_nibble(hex[i]);
        const auto lo = parse_nibble(hex[i + 1U]);
        if (hi < 0 || lo < 0) {
            out.clear();
            return out;
        }

        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }

    return out;
}

std::string json_escape(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (const char ch : text) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\r':
                escaped += "\\r";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

const char* verdict_name(compliance_dsl::Verdict verdict) {
    switch (verdict) {
        case compliance_dsl::Verdict::fail:
            return "FAIL";
        case compliance_dsl::Verdict::warn:
            return "WARN";
        case compliance_dsl::Verdict::info:
            return "INFO";
    }

    return "UNKNOWN";
}

const char* verdict_name(compliance_proto::Verdict verdict) {
    switch (verdict) {
        case compliance_proto::VERDICT_FAIL:
            return "FAIL";
        case compliance_proto::VERDICT_WARN:
            return "WARN";
        case compliance_proto::VERDICT_INFO:
            return "INFO";
        case compliance_proto::VERDICT_UNSPECIFIED:
            return "UNSPECIFIED";
        default:
            return "UNKNOWN";
    }
}

struct FindingCounts {
    std::size_t fail{0};
    std::size_t warn{0};
    std::size_t info{0};
};

FindingCounts count_findings(std::span<const compliance_dsl::Finding> findings) {
    FindingCounts counts;
    for (const auto& finding : findings) {
        switch (finding.verdict) {
            case compliance_dsl::Verdict::fail:
                ++counts.fail;
                break;
            case compliance_dsl::Verdict::warn:
                ++counts.warn;
                break;
            case compliance_dsl::Verdict::info:
                ++counts.info;
                break;
        }
    }
    return counts;
}

void print_findings_pretty(std::span<const compliance_dsl::Finding> findings) {
    const FindingCounts counts = count_findings(findings);

    if (findings.empty()) {
        std::cout << "PASS: no findings\n";
        return;
    }

    for (const auto& finding : findings) {
        std::cout << std::format(
            "{} {}: {}",
            verdict_name(finding.verdict),
            finding.test_id,
            finding.message);
        if (finding.reference.has_value()) {
            std::cout << std::format(" [{}]", *finding.reference);
        }
        std::cout << '\n';
    }

    std::cout << std::format(
        "Summary: {} fail, {} warn, {} info\n",
        counts.fail,
        counts.warn,
        counts.info);
}

void print_findings_json(std::span<const compliance_dsl::Finding> findings) {
    const FindingCounts counts = count_findings(findings);

    std::cout << "{"
              << "\"findingCount\":" << findings.size() << ','
              << "\"failCount\":" << counts.fail << ','
              << "\"warnCount\":" << counts.warn << ','
              << "\"infoCount\":" << counts.info << ','
              << "\"findings\":[";

    for (std::size_t index = 0; index < findings.size(); ++index) {
        const auto& finding = findings[index];
        if (index > 0) {
            std::cout << ',';
        }

        std::cout << "{"
                  << "\"testId\":\"" << json_escape(finding.test_id) << "\","
                  << "\"verdict\":\"" << verdict_name(finding.verdict) << "\","
                  << "\"message\":\"" << json_escape(finding.message) << "\"";
        if (finding.reference.has_value()) {
            std::cout << ",\"reference\":\"" << json_escape(*finding.reference) << "\"";
        } else {
            std::cout << ",\"reference\":null";
        }
        std::cout << "}";
    }

    std::cout << "]}\n";
}

std::vector<compliance_dsl::Rule> load_rules(const ComplianceOptions& opts) {
    if (opts.rule_path.empty() == opts.rules_dir.empty()) {
        throw std::runtime_error("Specify exactly one of --rule or --rules-dir");
    }

    if (!opts.rule_path.empty()) {
        std::vector<compliance_dsl::Rule> rules;
        auto loaded = compliance_dsl::load_rule_file(opts.rule_path);
        rules.push_back(std::move(loaded.rule));
        return rules;
    }

    std::vector<compliance_dsl::Rule> rules;
    for (auto& loaded_rule : compliance_dsl::load_rules_from_directory(opts.rules_dir)) {
        rules.push_back(std::move(loaded_rule.rule));
    }
    return rules;
}

std::unique_ptr<compliance_proto::ComplianceService::Stub> make_stub(std::string_view server) {
    auto channel = grpc::CreateChannel(std::string(server), grpc::InsecureChannelCredentials());
    return compliance_proto::ComplianceService::NewStub(channel);
}

FindingCounts count_remote_findings(
    const google::protobuf::RepeatedPtrField<compliance_proto::ComplianceFinding>& findings) {
    FindingCounts counts;
    for (const auto& finding : findings) {
        switch (finding.verdict()) {
            case compliance_proto::VERDICT_FAIL:
                ++counts.fail;
                break;
            case compliance_proto::VERDICT_WARN:
                ++counts.warn;
                break;
            case compliance_proto::VERDICT_INFO:
                ++counts.info;
                break;
            case compliance_proto::VERDICT_UNSPECIFIED:
                break;
            default:
                break;
        }
    }
    return counts;
}

void print_remote_result_pretty(const compliance_proto::ComplianceRunResult& result) {
    const FindingCounts counts = count_remote_findings(result.findings());

    std::cout << std::format(
        "Target {} on unit {}: evaluated {} retained devices across {} rules\n",
        result.target_id(),
        result.unit_id(),
        result.evaluated_device_count(),
        result.rule_count());

    if (result.findings().empty()) {
        std::cout << "PASS: no findings\n";
        return;
    }

    for (const auto& finding : result.findings()) {
        std::cout << std::format(
            "{} {} on {}: {}",
            verdict_name(finding.verdict()),
            finding.rule_id(),
            finding.observed_device_name().empty()
                ? finding.observed_device_id()
                : finding.observed_device_name(),
            finding.message());
        if (!finding.reference().empty()) {
            std::cout << std::format(" [{}]", finding.reference());
        }
        std::cout << '\n';
    }

    std::cout << std::format(
        "Summary: {} fail, {} warn, {} info\n",
        counts.fail,
        counts.warn,
        counts.info);
}

void print_remote_result_json(const compliance_proto::ComplianceRunResult& result) {
    const FindingCounts counts = count_remote_findings(result.findings());

    std::cout << "{"
              << "\"unitId\":\"" << json_escape(result.unit_id()) << "\","
              << "\"targetId\":\"" << json_escape(result.target_id()) << "\","
              << "\"ruleCount\":" << result.rule_count() << ','
              << "\"evaluatedDeviceCount\":" << result.evaluated_device_count() << ','
              << "\"failCount\":" << counts.fail << ','
              << "\"warnCount\":" << counts.warn << ','
              << "\"infoCount\":" << counts.info << ','
              << "\"findings\":[";

    for (int index = 0; index < result.findings_size(); ++index) {
        const auto& finding = result.findings(index);
        if (index > 0) {
            std::cout << ',';
        }
        std::cout << "{"
                  << "\"ruleId\":\"" << json_escape(finding.rule_id()) << "\","
                  << "\"verdict\":\"" << verdict_name(finding.verdict()) << "\","
                  << "\"message\":\"" << json_escape(finding.message()) << "\","
                  << "\"reference\":"
                  << (finding.reference().empty()
                      ? "null"
                      : ("\"" + json_escape(finding.reference()) + "\"")) << ','
                  << "\"observedDeviceId\":\"" << json_escape(finding.observed_device_id()) << "\","
                  << "\"observedDeviceName\":\"" << json_escape(finding.observed_device_name()) << "\""
                  << "}";
    }

    std::cout << "]}\n";
}

int exit_code_for_remote_result(const compliance_proto::ComplianceRunResult& result) {
    const FindingCounts counts = count_remote_findings(result.findings());
    return counts.fail > 0 ? 1 : 0;
}

} // namespace

int run_compliance_eval(const ComplianceOptions& opts) {
    if (opts.raw_data_hex.empty()) {
        std::cerr << "auracle: compliance eval requires --raw <hex>\n";
        return 2;
    }

    const auto raw_data = hex_to_bytes(opts.raw_data_hex);
    const auto raw_scan_response = hex_to_bytes(opts.raw_scan_response_hex);

    if (raw_data.empty() || (!opts.raw_scan_response_hex.empty() && raw_scan_response.empty())) {
        std::cerr << "auracle: failed to parse hex payload\n";
        return 2;
    }

    try {
        const auto rules = load_rules(opts);
        if (rules.empty()) {
            std::cerr << "auracle: no rule files found\n";
            return 2;
        }

        if (opts.verbose && !raw_scan_response.empty()) {
            std::cerr << "auracle: note: scan response bytes are ignored for EA-only rules\n";
        }

        observation::BleAdvertisement advertisement;
        advertisement.raw_data = raw_data;
        advertisement.raw_scan_response = raw_scan_response;

        const auto findings = compliance_dsl::evaluate_rules(rules, advertisement);

        switch (opts.format) {
            case OutputFormat::Pretty:
                print_findings_pretty(findings);
                break;
            case OutputFormat::Json:
            case OutputFormat::Prototext:
                print_findings_json(findings);
                break;
        }

        const auto counts = count_findings(findings);
        return counts.fail > 0 ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << std::format("auracle: compliance evaluation failed: {}\n", error.what());
        return 3;
    }
}

int run_compliance_list_rules(const ComplianceOptions& opts) {
    auto stub = make_stub(opts.server);
    grpc::ClientContext context;
    compliance_proto::ListComplianceRulesResponse response;
    const auto status = stub->ListComplianceRules(
        &context,
        compliance_proto::ListComplianceRulesRequest{},
        &response);

    if (!status.ok()) {
        std::cerr << std::format(
            "auracle: failed to list compliance rules: [{}] {}\n",
            static_cast<int>(status.error_code()),
            status.error_message());
        return 3;
    }

    if (opts.format == OutputFormat::Pretty) {
        for (const auto& rule : response.rules()) {
            std::cout << std::format("{} [{}] {}", rule.id(), verdict_name(rule.verdict()), rule.title());
            if (!rule.reference().empty()) {
                std::cout << std::format(" ({})", rule.reference());
            }
            std::cout << '\n';
        }
    } else {
        std::cout << "{\"rules\":[";
        for (int i = 0; i < response.rules_size(); ++i) {
            const auto& rule = response.rules(i);
            if (i > 0) {
                std::cout << ',';
            }
            std::cout << "{"
                      << "\"id\":\"" << json_escape(rule.id()) << "\","
                      << "\"title\":\"" << json_escape(rule.title()) << "\","
                      << "\"verdict\":\"" << verdict_name(rule.verdict()) << "\","
                      << "\"message\":\"" << json_escape(rule.message()) << "\","
                      << "\"reference\":"
                      << (rule.reference().empty()
                          ? "null"
                          : ("\"" + json_escape(rule.reference()) + "\""))
                      << "}";
        }
        std::cout << "]}\n";
    }

    return 0;
}

int run_compliance_list_suites(const ComplianceOptions& opts) {
    auto stub = make_stub(opts.server);
    grpc::ClientContext context;
    compliance_proto::ListComplianceSuitesResponse response;
    const auto status = stub->ListComplianceSuites(
        &context,
        compliance_proto::ListComplianceSuitesRequest{},
        &response);

    if (!status.ok()) {
        std::cerr << std::format(
            "auracle: failed to list compliance suites: [{}] {}\n",
            static_cast<int>(status.error_code()),
            status.error_message());
        return 3;
    }

    if (opts.format == OutputFormat::Pretty) {
        for (const auto& suite : response.suites()) {
            std::cout << std::format("{} {} ({} rules)\n", suite.id(), suite.title(), suite.rule_ids_size());
        }
    } else {
        std::cout << "{\"suites\":[";
        for (int i = 0; i < response.suites_size(); ++i) {
            const auto& suite = response.suites(i);
            if (i > 0) {
                std::cout << ',';
            }
            std::cout << "{"
                      << "\"id\":\"" << json_escape(suite.id()) << "\","
                      << "\"title\":\"" << json_escape(suite.title()) << "\","
                      << "\"ruleIds\":[";
            for (int rule_index = 0; rule_index < suite.rule_ids_size(); ++rule_index) {
                if (rule_index > 0) {
                    std::cout << ',';
                }
                std::cout << "\"" << json_escape(suite.rule_ids(rule_index)) << "\"";
            }
            std::cout << "]}";
        }
        std::cout << "]}\n";
    }

    return 0;
}

int run_compliance_run_rule(const ComplianceOptions& opts) {
    auto stub = make_stub(opts.server);
    grpc::ClientContext context;
    compliance_proto::RunComplianceRuleResponse response;
    compliance_proto::RunComplianceRuleRequest request;
    request.set_unit_id(opts.unit_id);
    request.set_rule_id(opts.rule_id);
    const auto status = stub->RunComplianceRule(
        &context,
        request,
        &response);

    if (!status.ok()) {
        std::cerr << std::format(
            "auracle: compliance run-rule failed: [{}] {}\n",
            static_cast<int>(status.error_code()),
            status.error_message());
        return 3;
    }

    switch (opts.format) {
        case OutputFormat::Pretty:
            print_remote_result_pretty(response.result());
            break;
        case OutputFormat::Json:
        case OutputFormat::Prototext:
            print_remote_result_json(response.result());
            break;
    }

    return exit_code_for_remote_result(response.result());
}

int run_compliance_run_suite(const ComplianceOptions& opts) {
    auto stub = make_stub(opts.server);
    grpc::ClientContext context;
    compliance_proto::RunComplianceSuiteResponse response;
    compliance_proto::RunComplianceSuiteRequest request;
    request.set_unit_id(opts.unit_id);
    request.set_suite_id(opts.suite_id);
    const auto status = stub->RunComplianceSuite(
        &context,
        request,
        &response);

    if (!status.ok()) {
        std::cerr << std::format(
            "auracle: compliance run-suite failed: [{}] {}\n",
            static_cast<int>(status.error_code()),
            status.error_message());
        return 3;
    }

    switch (opts.format) {
        case OutputFormat::Pretty:
            print_remote_result_pretty(response.result());
            break;
        case OutputFormat::Json:
        case OutputFormat::Prototext:
            print_remote_result_json(response.result());
            break;
    }

    return exit_code_for_remote_result(response.result());
}

} // namespace auracle::cli
