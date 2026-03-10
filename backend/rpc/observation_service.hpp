#pragma once

#include <observation/scanner_manager.hpp>

#include <auracle/observation/v1/observation.grpc.pb.h>

#include <grpcpp/grpcpp.h>

namespace auracle::rpc {

namespace obs_proto = ::auracle::observation::v1;

class ObservationServiceImpl final : public obs_proto::ObservationService::Service {
public:
    explicit ObservationServiceImpl(observation::ScannerManager& manager);

    grpc::Status StartScan(
        grpc::ServerContext* context,
        const obs_proto::StartScanRequest* request,
        obs_proto::StartScanResponse* response) override;

    grpc::Status StopScan(
        grpc::ServerContext* context,
        const obs_proto::StopScanRequest* request,
        obs_proto::StopScanResponse* response) override;

    grpc::Status WatchObservations(
        grpc::ServerContext* context,
        const obs_proto::WatchObservationsRequest* request,
        grpc::ServerWriter<obs_proto::ObservationEvent>* writer) override;

private:
    observation::ScannerManager& manager_;
};

} // namespace auracle::rpc
