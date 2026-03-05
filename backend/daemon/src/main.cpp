#include <inventory/inventory.hpp>
#include <inventory/providers/serial_provider.hpp>
#include <rpc/inventory_service.hpp>

#include <dts/log.hpp>
#include <dts/serial.hpp>

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void signal_handler(int /*sig*/) {
    g_running.store(false, std::memory_order_relaxed);
}

constexpr std::string_view log_tag = "daemon";

} // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    dts::log::set_min_level(dts::log::level::debug);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    dts::log::info(log_tag, "auracle-daemon starting");

    // --- inventory layer ---
    auracle::inventory::InventoryRegistry registry;

    dts::serial::monitor serial_monitor;
    auracle::inventory::SerialCandidateProvider serial_provider(serial_monitor, registry);

    serial_provider.start();
    serial_monitor.start(dts::serial::start_options{.emit_initial_snapshot = true});

    // --- gRPC server ---
    auracle::rpc::InventoryServiceImpl inventory_svc(registry);

    const std::string listen_addr = "0.0.0.0:50051";
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&inventory_svc);

    auto server = builder.BuildAndStart();
    dts::log::info(log_tag, "gRPC server listening on {}", listen_addr);

    // --- wait for signal ---
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    dts::log::info(log_tag, "shutting down");

    server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(2));
    serial_monitor.stop();
    serial_provider.stop();

    return EXIT_SUCCESS;
}
