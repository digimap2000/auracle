#include "watch.hpp"
#include "render.hpp"

#include <auracle/inventory/v1/inventory.grpc.pb.h>

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <csignal>
#include <format>
#include <iostream>
#include <memory>

namespace auracle::cli {

namespace proto = ::auracle::inventory::v1;

namespace {

std::atomic<grpc::ClientContext*> g_context{nullptr};

void signal_handler(int /*sig*/) {
    if (auto* ctx = g_context.load(std::memory_order_relaxed)) {
        ctx->TryCancel();
    }
}

} // namespace

int run_watch(const WatchOptions& opts) {
    auto renderer = make_renderer(opts.format);

    auto channel = grpc::CreateChannel(
        opts.server, grpc::InsecureChannelCredentials());
    auto stub = proto::InventoryService::NewStub(channel);

    proto::WatchRequest request;
    request.set_include_initial_snapshot(opts.initial);
    request.set_include_candidates(opts.include_candidates);
    request.set_include_units(opts.include_units);

    grpc::ClientContext context;
    g_context.store(&context, std::memory_order_relaxed);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (opts.verbose) {
        std::cerr << std::format("connecting to {}...\n", opts.server);
    }

    auto reader = stub->Watch(&context, request);

    proto::InventoryEvent event;
    while (reader->Read(&event)) {
        renderer->render(event);
    }

    g_context.store(nullptr, std::memory_order_relaxed);

    auto status = reader->Finish();
    if (status.ok() || status.error_code() == grpc::StatusCode::CANCELLED) {
        return 0;
    }

    std::cerr << std::format("auracle: unable to connect to server {}\n", opts.server);
    return 3;
}

} // namespace auracle::cli
