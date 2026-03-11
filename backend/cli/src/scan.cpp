#include "scan.hpp"

#include <auracle/observation/v1/observation.grpc.pb.h>

#include <google/protobuf/util/json_util.h>
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <csignal>
#include <format>
#include <iostream>

namespace auracle::cli {

namespace obs_proto = ::auracle::observation::v1;

namespace {

std::atomic<grpc::ClientContext*> g_scan_context{nullptr};

void scan_signal_handler(int /*sig*/) {
    if (auto* ctx = g_scan_context.load(std::memory_order_relaxed)) {
        ctx->TryCancel();
    }
}

void print_advertisement_pretty(const obs_proto::ObservationEvent& event) {
    const auto& adv = event.ble_advertisement();
    std::string name_str = adv.name().empty() ? "(unknown)" : adv.name();

    std::cout << std::format("{:>4} dBm  {}  {}", adv.rssi(), adv.device_id(), name_str);

    if (adv.company_id() != 0) {
        std::cout << std::format("  company=0x{:04X}", adv.company_id());
    }

    if (adv.service_uuids_size() > 0) {
        std::cout << "  svc=[";
        for (int i = 0; i < adv.service_uuids_size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << adv.service_uuids(i);
        }
        std::cout << "]";
    }

    std::cout << "\n";
}

void print_advertisement_json(const obs_proto::ObservationEvent& event) {
    google::protobuf::util::JsonPrintOptions options;
    options.always_print_fields_with_no_presence = true;
    std::string json;
    auto status = google::protobuf::util::MessageToJsonString(event, &json, options);
    if (status.ok()) {
        std::cout << json << '\n';
    }
}

void print_observed_device_pretty(const obs_proto::ObservedBleDevice& device) {
    const auto& adv = device.advertisement();
    std::string name_str = adv.name().empty() ? "(unknown)" : adv.name();

    std::cout << std::format(
        "{:>4} dBm  {}  {}  seen={} last={}ms",
        adv.rssi(),
        adv.device_id(),
        name_str,
        device.seen_count(),
        device.last_seen_ms());

    if (adv.company_id() != 0) {
        std::cout << std::format("  company=0x{:04X}", adv.company_id());
    }

    if (adv.service_uuids_size() > 0) {
        std::cout << "  svc=[";
        for (int i = 0; i < adv.service_uuids_size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << adv.service_uuids(i);
        }
        std::cout << "]";
    }

    std::cout << "\n";
}

void print_observed_device_json(const obs_proto::ObservedBleDevice& device) {
    google::protobuf::util::JsonPrintOptions options;
    options.always_print_fields_with_no_presence = true;
    std::string json;
    auto status = google::protobuf::util::MessageToJsonString(device, &json, options);
    if (status.ok()) {
        std::cout << json << '\n';
    }
}

} // namespace

int run_scan(const ScanOptions& opts) {
    auto channel = grpc::CreateChannel(
        opts.server, grpc::InsecureChannelCredentials());
    auto stub = obs_proto::ObservationService::NewStub(channel);

    if (opts.verbose) {
        std::cerr << std::format("connecting to {}...\n", opts.server);
    }

    // Start scan on the unit
    {
        obs_proto::StartScanRequest request;
        request.set_unit_id(opts.unit_id);
        request.set_allow_duplicates(opts.allow_duplicates);

        grpc::ClientContext context;
        obs_proto::StartScanResponse response;
        auto status = stub->StartScan(&context, request, &response);

        if (!status.ok()) {
            std::cerr << std::format("auracle: failed to start scan on unit {}: {}\n",
                opts.unit_id, status.error_message());
            return 3;
        }
    }

    if (opts.verbose) {
        std::cerr << std::format("scanning on unit {}...\n", opts.unit_id);
    }

    // Stream observations
    obs_proto::WatchObservationsRequest watch_request;
    watch_request.set_unit_id(opts.unit_id);
    watch_request.set_include_initial_snapshot(opts.include_initial_snapshot);

    grpc::ClientContext watch_context;
    g_scan_context.store(&watch_context, std::memory_order_relaxed);

    std::signal(SIGINT, scan_signal_handler);
    std::signal(SIGTERM, scan_signal_handler);

    auto reader = stub->WatchObservations(&watch_context, watch_request);

    obs_proto::ObservationEvent event;
    while (reader->Read(&event)) {
        if (event.has_ble_advertisement()) {
            switch (opts.format) {
            case OutputFormat::Pretty:
                print_advertisement_pretty(event);
                break;
            case OutputFormat::Json:
                print_advertisement_json(event);
                break;
            case OutputFormat::Prototext:
                // Reuse json for prototext — simple for now
                print_advertisement_json(event);
                break;
            }
        }
    }

    g_scan_context.store(nullptr, std::memory_order_relaxed);

    // Stop scan on cleanup
    {
        obs_proto::StopScanRequest request;
        request.set_unit_id(opts.unit_id);

        grpc::ClientContext context;
        obs_proto::StopScanResponse response;
        stub->StopScan(&context, request, &response);
    }

    auto status = reader->Finish();
    if (status.ok() || status.error_code() == grpc::StatusCode::CANCELLED) {
        return 0;
    }

    std::cerr << std::format("auracle: stream error: {}\n", status.error_message());
    return 3;
}

int run_scan_list(const ScanOptions& opts) {
    auto channel = grpc::CreateChannel(
        opts.server, grpc::InsecureChannelCredentials());
    auto stub = obs_proto::ObservationService::NewStub(channel);

    if (opts.verbose) {
        std::cerr << std::format("querying retained scan results from {}...\n", opts.server);
    }

    obs_proto::ListObservedDevicesRequest request;
    request.set_unit_id(opts.unit_id);

    grpc::ClientContext context;
    obs_proto::ListObservedDevicesResponse response;
    auto status = stub->ListObservedDevices(&context, request, &response);

    if (!status.ok()) {
        std::cerr << std::format(
            "auracle: failed to list scan results for unit {}: {}\n",
            opts.unit_id,
            status.error_message());
        return 3;
    }

    for (const auto& device : response.devices()) {
        switch (opts.format) {
        case OutputFormat::Pretty:
            print_observed_device_pretty(device);
            break;
        case OutputFormat::Json:
        case OutputFormat::Prototext:
            print_observed_device_json(device);
            break;
        }
    }

    return 0;
}

} // namespace auracle::cli
