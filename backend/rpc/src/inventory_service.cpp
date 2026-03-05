#include "inventory_service.hpp"
#include "convert.hpp"

#include <dts/channel.hpp>

#include <chrono>
#include <thread>
#include <variant>

namespace auracle::rpc {

InventoryServiceImpl::InventoryServiceImpl(inventory::InventoryRegistry& registry)
    : registry_(registry) {}

grpc::Status InventoryServiceImpl::Watch(
    grpc::ServerContext* context,
    const proto::WatchRequest* request,
    grpc::ServerWriter<proto::InventoryEvent>* writer) {

    // Channel bridges events from signal callbacks to this RPC thread.
    dts::channel<inventory::InventoryEvent> events;

    auto conn = registry_.on_event.connect(
        [&events](const inventory::InventoryEvent& event) {
            events.try_send(event);
        });

    // Send initial snapshot if requested.
    if (request->include_initial_snapshot()) {
        if (request->include_candidates()) {
            for (const auto& candidate : registry_.list_candidates()) {
                proto::InventoryEvent proto_event;
                inventory::InventoryEvent domain{inventory::CandidateAdded{candidate}};
                to_proto(domain, &proto_event);
                if (!writer->Write(proto_event)) {
                    conn.disconnect();
                    return grpc::Status::OK;
                }
            }
        }
        if (request->include_units()) {
            for (const auto& unit : registry_.list_units()) {
                proto::InventoryEvent proto_event;
                inventory::InventoryEvent domain{inventory::UnitAdded{unit}};
                to_proto(domain, &proto_event);
                if (!writer->Write(proto_event)) {
                    conn.disconnect();
                    return grpc::Status::OK;
                }
            }
        }
    }

    // Watch for client cancellation in a background thread.
    std::jthread cancel_watcher([&](std::stop_token stop) {
        while (!stop.stop_requested() && !context->IsCancelled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        events.close();
    });

    // Stream events until client disconnects or cancels.
    inventory::InventoryEvent domain_event;
    while (events.recv(domain_event)) {
        const bool is_candidate = std::visit([](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            return std::is_same_v<T, inventory::CandidateAdded>
                || std::is_same_v<T, inventory::CandidateUpdated>
                || std::is_same_v<T, inventory::CandidateGone>;
        }, domain_event);

        if (is_candidate && !request->include_candidates()) continue;
        if (!is_candidate && !request->include_units()) continue;

        proto::InventoryEvent proto_event;
        to_proto(domain_event, &proto_event);

        if (!writer->Write(proto_event)) break;
    }

    conn.disconnect();
    return grpc::Status::OK;
}

} // namespace auracle::rpc
