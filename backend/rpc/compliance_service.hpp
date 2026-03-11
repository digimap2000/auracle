#pragma once

#include <compliance/repository.hpp>
#include <observation/scanner_manager.hpp>

#include <auracle/compliance/v1/compliance.grpc.pb.h>

#include <grpcpp/grpcpp.h>

namespace auracle::rpc {

namespace compliance_proto = ::auracle::compliance::v1;

class ComplianceServiceImpl final : public compliance_proto::ComplianceService::Service {
public:
    ComplianceServiceImpl(
        const compliance::Repository& repository,
        observation::ScannerManager& scanner_manager);

    grpc::Status ListComplianceRules(
        grpc::ServerContext* context,
        const compliance_proto::ListComplianceRulesRequest* request,
        compliance_proto::ListComplianceRulesResponse* response) override;

    grpc::Status ListComplianceSuites(
        grpc::ServerContext* context,
        const compliance_proto::ListComplianceSuitesRequest* request,
        compliance_proto::ListComplianceSuitesResponse* response) override;

    grpc::Status RunComplianceRule(
        grpc::ServerContext* context,
        const compliance_proto::RunComplianceRuleRequest* request,
        compliance_proto::RunComplianceRuleResponse* response) override;

    grpc::Status RunComplianceSuite(
        grpc::ServerContext* context,
        const compliance_proto::RunComplianceSuiteRequest* request,
        compliance_proto::RunComplianceSuiteResponse* response) override;

private:
    const compliance::Repository& repository_;
    observation::ScannerManager& scanner_manager_;
};

} // namespace auracle::rpc
