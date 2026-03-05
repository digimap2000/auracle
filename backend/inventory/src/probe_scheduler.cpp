#include "probe_scheduler.hpp"

#include <algorithm>
#include <ranges>
#include <variant>

namespace auracle::inventory {

ProbeScheduler::ProbeScheduler(
    InventoryRegistry& registry,
    std::chrono::milliseconds reprobe_interval)
    : registry_(registry)
    , reprobe_interval_(reprobe_interval) {}

void ProbeScheduler::add_prober(Transport transport, std::unique_ptr<IProber> prober) {
    probers_.emplace_back(transport, std::move(prober));
}

void ProbeScheduler::start() {
    event_conn_ = dts::scoped_connection{registry_.on_event.connect(
        [this](const InventoryEvent& event) { on_inventory_event(event); })};

    reprobe_thread_ = std::jthread([this](std::stop_token stop) {
        reprobe_loop(stop);
    });
}

void ProbeScheduler::stop() {
    event_conn_ = dts::scoped_connection{};
    if (reprobe_thread_.joinable()) {
        reprobe_thread_.request_stop();
        reprobe_thread_.join();
    }
}

void ProbeScheduler::on_inventory_event(const InventoryEvent& event) {
    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, CandidateAdded>) {
            probe_candidate(e.candidate);
        } else if constexpr (std::is_same_v<T, CandidateUpdated>) {
            probe_candidate(e.candidate);
        }
    }, event);
}

void ProbeScheduler::probe_candidate(const HardwareCandidate& candidate) {
    for (auto& [transport, prober] : probers_) {
        if (transport == candidate.transport) {
            auto result = prober->probe(candidate);
            registry_.submit_probe_result(candidate.id, result);
            return;
        }
    }
}

void ProbeScheduler::reprobe_loop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        // Interruptible sleep
        for (int i = 0; i < static_cast<int>(reprobe_interval_.count() / 100)
                 && !stop.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (stop.stop_requested()) {
            break;
        }

        auto candidates = registry_.list_candidates(false);
        for (const auto& candidate : candidates) {
            if (stop.stop_requested()) {
                break;
            }
            probe_candidate(candidate);
        }
    }
}

} // namespace auracle::inventory
