#include <inventory/inventory.hpp>
#include <inventory/probe_scheduler.hpp>
#include <inventory/probers/host_bluetooth_prober.hpp>
#include <inventory/probers/mdns_agent_prober.hpp>
#include <inventory/providers/host_bluetooth_provider.hpp>
#include <inventory/providers/mdns_provider.hpp>
#include <inventory/providers/serial_provider.hpp>
#include <rpc/inventory_service.hpp>

#include <dts/bluetooth.hpp>
#include <dts/log.hpp>
#include <dts/mdns.hpp>
#include <dts/serial.hpp>

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
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

    // Serial provider
    dts::serial::monitor serial_monitor;
    auracle::inventory::SerialCandidateProvider serial_provider(serial_monitor, registry);

    // Host Bluetooth provider
    dts::bluetooth::monitor bt_monitor;
    auracle::inventory::HostBluetoothCandidateProvider bt_provider(bt_monitor, registry);

    // mDNS provider
    dts::mdns::monitor mdns_monitor(dts::mdns::browse_query{.service_type = "_auracle._tcp"});
    auracle::inventory::MdnsCandidateProvider mdns_provider(mdns_monitor, registry);

    // Probe scheduler
    auracle::inventory::ProbeScheduler probe_scheduler(registry);
    probe_scheduler.add_prober(
        auracle::inventory::Transport::HostBluetooth,
        std::make_unique<auracle::inventory::HostBluetoothProber>());
    probe_scheduler.add_prober(
        auracle::inventory::Transport::Mdns,
        std::make_unique<auracle::inventory::MdnsAgentProber>());

    // Start everything
    serial_provider.start();
    serial_monitor.start(dts::serial::start_options{.emit_initial_snapshot = true});
    bt_provider.start();
    bt_monitor.start(dts::bluetooth::start_options{.emit_initial_snapshot = true});
    mdns_provider.start();
    mdns_monitor.start();
    probe_scheduler.start();

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
    probe_scheduler.stop();
    mdns_monitor.stop();
    mdns_provider.stop();
    bt_monitor.stop();
    bt_provider.stop();
    serial_monitor.stop();
    serial_provider.stop();

    return EXIT_SUCCESS;
}
