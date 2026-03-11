#include "compliance_service.hpp"

#include <compliance/engine.hpp>
#include <compliance/normalizer.hpp>

#include <span>

namespace auracle::rpc {
namespace {

compliance_proto::Verdict to_proto_verdict(compliance::Verdict verdict) {
    switch (verdict) {
        case compliance::Verdict::fail:
            return compliance_proto::VERDICT_FAIL;
        case compliance::Verdict::warn:
            return compliance_proto::VERDICT_WARN;
        case compliance::Verdict::info:
            return compliance_proto::VERDICT_INFO;
    }

    return compliance_proto::VERDICT_UNSPECIFIED;
}

void to_proto(
    const compliance::LoadedRule& loaded_rule,
    compliance_proto::ComplianceRule* out) {
    out->set_id(loaded_rule.rule.id);
    out->set_title(loaded_rule.rule.title.value_or(""));
    out->set_verdict(to_proto_verdict(loaded_rule.rule.verdict));
    out->set_message(loaded_rule.rule.message);
    out->set_reference(loaded_rule.rule.reference.value_or(""));
}

void to_proto(
    const compliance::LoadedSuite& loaded_suite,
    compliance_proto::ComplianceSuite* out) {
    out->set_id(loaded_suite.suite.id);
    out->set_title(loaded_suite.suite.title.value_or(""));
    for (const std::string& rule_id : loaded_suite.suite.rule_ids) {
        out->add_rule_ids(rule_id);
    }
}

template<typename RuleRange>
void populate_run_result(
    const std::string& unit_id,
    std::string target_id,
    const RuleRange& rules,
    observation::ScannerManager& scanner_manager,
    compliance_proto::ComplianceRunResult* out) {
    out->set_unit_id(unit_id);
    out->set_target_id(std::move(target_id));

    const auto observed_devices = scanner_manager.list_observed_devices(unit_id);
    out->set_evaluated_device_count(static_cast<std::uint32_t>(observed_devices.size()));
    out->set_rule_count(static_cast<std::uint32_t>(std::size(rules)));

    for (const auto& observed_device : observed_devices) {
        for (const auto& rule_ref : rules) {
            const compliance::Rule& rule = rule_ref.get().rule;
            if (auto finding = compliance::evaluate_rule(
                    rule,
                    compliance::normalize_ea_facts(observed_device.advertisement));
                finding.has_value()) {
                auto* out_finding = out->add_findings();
                out_finding->set_rule_id(finding->test_id);
                out_finding->set_verdict(to_proto_verdict(finding->verdict));
                out_finding->set_message(finding->message);
                out_finding->set_reference(finding->reference.value_or(""));
                out_finding->set_observed_device_id(observed_device.advertisement.device_id);
                out_finding->set_observed_device_name(observed_device.advertisement.name);
            }
        }
    }
}

grpc::Status validate_run_request(std::string_view unit_id, std::string_view target) {
    if (unit_id.empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "unit_id is required");
    }
    if (target.empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "target id is required");
    }
    return grpc::Status::OK;
}

} // namespace

ComplianceServiceImpl::ComplianceServiceImpl(
    const compliance::Repository& repository,
    observation::ScannerManager& scanner_manager)
    : repository_(repository),
      scanner_manager_(scanner_manager) {}

grpc::Status ComplianceServiceImpl::ListComplianceRules(
    grpc::ServerContext* /*context*/,
    const compliance_proto::ListComplianceRulesRequest* /*request*/,
    compliance_proto::ListComplianceRulesResponse* response) {
    for (const compliance::LoadedRule& loaded_rule : repository_.rules()) {
        to_proto(loaded_rule, response->add_rules());
    }
    return grpc::Status::OK;
}

grpc::Status ComplianceServiceImpl::ListComplianceSuites(
    grpc::ServerContext* /*context*/,
    const compliance_proto::ListComplianceSuitesRequest* /*request*/,
    compliance_proto::ListComplianceSuitesResponse* response) {
    for (const compliance::LoadedSuite& loaded_suite : repository_.suites()) {
        to_proto(loaded_suite, response->add_suites());
    }
    return grpc::Status::OK;
}

grpc::Status ComplianceServiceImpl::RunComplianceRule(
    grpc::ServerContext* /*context*/,
    const compliance_proto::RunComplianceRuleRequest* request,
    compliance_proto::RunComplianceRuleResponse* response) {
    if (grpc::Status status = validate_run_request(request->unit_id(), request->rule_id()); !status.ok()) {
        return status;
    }

    const compliance::LoadedRule* loaded_rule = repository_.find_rule(request->rule_id());
    if (loaded_rule == nullptr) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "unknown compliance rule");
    }

    const std::array rules{std::cref(*loaded_rule)};
    populate_run_result(
        request->unit_id(),
        request->rule_id(),
        rules,
        scanner_manager_,
        response->mutable_result());

    return grpc::Status::OK;
}

grpc::Status ComplianceServiceImpl::RunComplianceSuite(
    grpc::ServerContext* /*context*/,
    const compliance_proto::RunComplianceSuiteRequest* request,
    compliance_proto::RunComplianceSuiteResponse* response) {
    if (grpc::Status status = validate_run_request(request->unit_id(), request->suite_id()); !status.ok()) {
        return status;
    }

    if (repository_.find_suite(request->suite_id()) == nullptr) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "unknown compliance suite");
    }

    const auto rules = repository_.rules_for_suite(request->suite_id());
    populate_run_result(
        request->unit_id(),
        request->suite_id(),
        rules,
        scanner_manager_,
        response->mutable_result());

    return grpc::Status::OK;
}

} // namespace auracle::rpc
