#include <inventory/inventory.hpp>
#include <inventory/probe_scheduler.hpp>
#include <inventory/probers/host_bluetooth_prober.hpp>
#include <inventory/probers/mdns_agent_prober.hpp>
#include <inventory/probers/serial_agent_prober.hpp>
#include <inventory/providers/host_bluetooth_provider.hpp>
#include <inventory/providers/mdns_provider.hpp>
#include <inventory/providers/serial_provider.hpp>
#include <observation/local_bluetooth_scanner.hpp>
#include <observation/remote_wire_scanner.hpp>
#include <observation/scanner_manager.hpp>
#include <rpc/inventory_service.hpp>
#include <rpc/observation_service.hpp>

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
#include <variant>

namespace {

std::atomic<bool> g_running{true};

void signal_handler(int /*sig*/) {
    g_running.store(false, std::memory_order_relaxed);
}

constexpr std::string_view log_tag = "daemon";

[[nodiscard]] bool has_capability(
    const auracle::inventory::HardwareUnit& unit,
    auracle::inventory::Capability capability) {
    return std::ranges::find(unit.capabilities, capability) != unit.capabilities.end();
}

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
    probe_scheduler.add_prober(
        auracle::inventory::Transport::Serial,
        std::make_unique<auracle::inventory::SerialAgentProber>());

    // --- observation layer ---
    auracle::observation::ScannerManager scanner_manager(registry);

    // Bind a scanner implementation when a scan-capable unit is promoted.
    auto unit_event_conn = registry.on_event.connect(
        [&scanner_manager, &registry](const auracle::inventory::InventoryEvent& event) {
            std::visit([&scanner_manager, &registry](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, auracle::inventory::UnitAdded>
                           || std::is_same_v<T, auracle::inventory::UnitUpdated>) {
                    if (!has_capability(e.unit, auracle::inventory::Capability::BleScan)) {
                        return;
                    }

                    if (e.unit.kind == auracle::inventory::HardwareKind::HostBluetoothAdapter) {
                        scanner_manager.bind_scanner(
                            e.unit.id.value,
                            std::make_unique<auracle::observation::LocalBluetoothScanner>(
                                std::make_unique<dts::bluetooth::scanner>()));
                        return;
                    }

                    if (e.unit.kind == auracle::inventory::HardwareKind::Nrf5340AudioDK) {
                        const auto candidate = registry.get_candidate(e.unit.bound_candidate);
                        if (!candidate.has_value()) {
                            dts::log::warn(log_tag, "missing candidate for unit {}", e.unit.id.value);
                            return;
                        }

                        scanner_manager.bind_scanner(
                            e.unit.id.value,
                            std::make_unique<auracle::observation::RemoteWireScanner>(*candidate));
                    }
                } else if constexpr (std::is_same_v<T, auracle::inventory::UnitRemoved>) {
                    scanner_manager.unbind_scanner(e.id.value);
                }
            }, event);
        });

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
    auracle::rpc::ObservationServiceImpl observation_svc(scanner_manager);

    const std::string listen_addr = "0.0.0.0:50051";
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&inventory_svc);
    builder.RegisterService(&observation_svc);

    auto server = builder.BuildAndStart();
    dts::log::info(log_tag, "gRPC server listening on {}", listen_addr);

    // --- wait for signal ---
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    dts::log::info(log_tag, "shutting down");

    server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(2));
    unit_event_conn.disconnect();
    probe_scheduler.stop();
    mdns_monitor.stop();
    mdns_provider.stop();
    bt_monitor.stop();
    bt_provider.stop();
    serial_monitor.stop();
    serial_provider.stop();

    return EXIT_SUCCESS;
}
