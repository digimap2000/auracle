#include "observation_service.hpp"
#include "observation_convert.hpp"

#include <dts/channel.hpp>

#include <chrono>
#include <thread>

namespace auracle::rpc {

ObservationServiceImpl::ObservationServiceImpl(observation::ScannerManager& manager)
    : manager_(manager) {}

grpc::Status ObservationServiceImpl::StartScan(
    grpc::ServerContext* /*context*/,
    const obs_proto::StartScanRequest* request,
    obs_proto::StartScanResponse* /*response*/) {

    auto result = manager_.start_scan(request->unit_id(), request->allow_duplicates());
    if (!result) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, result.error());
    }
    return grpc::Status::OK;
}

grpc::Status ObservationServiceImpl::StopScan(
    grpc::ServerContext* /*context*/,
    const obs_proto::StopScanRequest* request,
    obs_proto::StopScanResponse* /*response*/) {

    auto result = manager_.stop_scan(request->unit_id());
    if (!result) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, result.error());
    }
    return grpc::Status::OK;
}

grpc::Status ObservationServiceImpl::WatchObservations(
    grpc::ServerContext* context,
    const obs_proto::WatchObservationsRequest* request,
    grpc::ServerWriter<obs_proto::ObservationEvent>* writer) {

    dts::channel<observation::ObservationEvent> events;

    const std::string filter_unit = request->unit_id();

    auto conn = manager_.on_observation.connect(
        [&events, &filter_unit](const observation::ObservationEvent& event) {
            if (filter_unit.empty() || event.unit_id == filter_unit) {
                events.try_send(event);
            }
        });

    // Watch for client cancellation in a background thread.
    std::jthread cancel_watcher([&](std::stop_token stop) {
        while (!stop.stop_requested() && !context->IsCancelled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        events.close();
    });

    // Stream events until client disconnects.
    observation::ObservationEvent domain_event;
    while (events.recv(domain_event)) {
        obs_proto::ObservationEvent proto_event;
        to_proto(domain_event, &proto_event);

        if (!writer->Write(proto_event)) break;
    }

    conn.disconnect();
    return grpc::Status::OK;
}

} // namespace auracle::rpc
