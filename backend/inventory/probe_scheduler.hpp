#pragma once

#include "probers/prober.hpp"
#include "registry.hpp"
#include "types.hpp"

#include <dts/signal.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace auracle::inventory {

class ProbeScheduler {
public:
    explicit ProbeScheduler(
        InventoryRegistry& registry,
        std::chrono::milliseconds reprobe_interval = std::chrono::seconds(10));

    void add_prober(Transport transport, std::unique_ptr<IProber> prober);
    void start();
    void stop();

private:
    void on_inventory_event(const InventoryEvent& event);
    void probe_candidate(const HardwareCandidate& candidate);
    void reprobe_loop(std::stop_token stop);

    InventoryRegistry& registry_;
    std::chrono::milliseconds reprobe_interval_;
    std::vector<std::pair<Transport, std::unique_ptr<IProber>>> probers_;
    dts::scoped_connection event_conn_;
    std::jthread reprobe_thread_;
};

} // namespace auracle::inventory
