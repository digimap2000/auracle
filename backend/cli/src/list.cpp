#include "list.hpp"

#include <auracle/inventory/v1/inventory.grpc.pb.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <grpcpp/grpcpp.h>

#include <format>
#include <iostream>

namespace auracle::cli {

namespace proto = ::auracle::inventory::v1;

namespace {

[[nodiscard]] std::string transport_name(proto::Transport t) {
    switch (t) {
    case proto::TRANSPORT_SERIAL:         return "serial";
    case proto::TRANSPORT_MDNS:           return "mdns";
    case proto::TRANSPORT_HOST_BLUETOOTH: return "bluetooth";
    default:                              return "unknown";
    }
}

[[nodiscard]] std::string kind_name(proto::HardwareKind k) {
    switch (k) {
    case proto::HARDWARE_KIND_NRF5340_AUDIO_DK:       return "nrf5340-audio-dk";
    case proto::HARDWARE_KIND_HOST_BLUETOOTH_ADAPTER:  return "host-bluetooth";
    default:                                           return "unknown";
    }
}

[[nodiscard]] std::string candidate_detail(const proto::HardwareCandidate& c) {
    if (c.has_serial()) {
        return c.serial().port();
    }
    if (c.has_mdns()) {
        return std::format("{}:{}", c.mdns().host(), c.mdns().port());
    }
    if (c.has_host_bluetooth()) {
        const auto& bt = c.host_bluetooth();
        return std::format("{} [{}] {}", bt.name(), bt.address(),
            bt.powered() ? "powered" : "off");
    }
    return {};
}

void print_candidates_pretty(const proto::ListCandidatesResponse& resp) {
    if (resp.candidates_size() == 0) {
        std::cout << "no candidates\n";
        return;
    }

    for (const auto& c : resp.candidates()) {
        std::cout << std::format("  {}  {:10s}  {}{}\n",
            c.id(),
            transport_name(c.transport()),
            candidate_detail(c),
            c.present() ? "" : "  (gone)");
    }
}

void print_units_pretty(const proto::ListUnitsResponse& resp) {
    if (resp.units_size() == 0) {
        std::cout << "no units\n";
        return;
    }

    for (const auto& u : resp.units()) {
        const auto& id = u.identity();
        std::string identity_str;
        if (!id.product().empty() || !id.serial().empty()) {
            identity_str = std::format("  {} serial={}",
                id.product(), id.serial());
            if (!id.firmware_version().empty()) {
                identity_str += std::format(" fw={}", id.firmware_version());
            }
        }
        std::cout << std::format("  {}  {:22s}{}{}\n",
            u.id(),
            kind_name(u.kind()),
            identity_str,
            u.present() ? "" : "  (offline)");
    }
}

template<typename T>
void print_json(const T& msg) {
    google::protobuf::util::JsonPrintOptions options;
    options.always_print_fields_with_no_presence = true;
    std::string json;
    auto status = google::protobuf::util::MessageToJsonString(msg, &json, options);
    if (status.ok()) {
        std::cout << json << '\n';
    }
}

template<typename T>
void print_prototext(const T& msg) {
    std::string text;
    google::protobuf::TextFormat::PrintToString(msg, &text);
    std::cout << text;
}

} // namespace

int run_list(const ListOptions& opts) {
    auto channel = grpc::CreateChannel(
        opts.server, grpc::InsecureChannelCredentials());
    auto stub = proto::InventoryService::NewStub(channel);

    if (opts.verbose) {
        std::cerr << std::format("connecting to {}...\n", opts.server);
    }

    if (opts.include_candidates) {
        proto::ListCandidatesRequest request;
        request.set_include_gone(opts.include_gone);

        grpc::ClientContext context;
        proto::ListCandidatesResponse response;
        auto status = stub->ListCandidates(&context, request, &response);

        if (!status.ok()) {
            std::cerr << std::format("auracle: unable to connect to server {}\n", opts.server);
            return 3;
        }

        switch (opts.format) {
        case OutputFormat::Pretty:
            std::cout << "candidates:\n";
            print_candidates_pretty(response);
            break;
        case OutputFormat::Json:
            print_json(response);
            break;
        case OutputFormat::Prototext:
            print_prototext(response);
            break;
        }
    }

    if (opts.include_units) {
        proto::ListUnitsRequest request;
        request.set_include_offline(opts.include_offline);

        grpc::ClientContext context;
        proto::ListUnitsResponse response;
        auto status = stub->ListUnits(&context, request, &response);

        if (!status.ok()) {
            std::cerr << std::format("auracle: unable to connect to server {}\n", opts.server);
            return 3;
        }

        switch (opts.format) {
        case OutputFormat::Pretty:
            std::cout << "units:\n";
            print_units_pretty(response);
            break;
        case OutputFormat::Json:
            print_json(response);
            break;
        case OutputFormat::Prototext:
            print_prototext(response);
            break;
        }
    }

    return 0;
}

} // namespace auracle::cli
