#pragma once

#include "provider.hpp"
#include "../candidate.hpp"
#include "../registry.hpp"
#include "../types.hpp"

#include <dts/mdns.hpp>
#include <dts/signal.hpp>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace auracle::inventory {

class MdnsCandidateProvider final : public ICandidateProvider {
public:
    MdnsCandidateProvider(dts::mdns::monitor& monitor, InventoryRegistry& registry);

    void start() override;
    void stop() override;

private:
    void on_arrived(const dts::mdns::service_info& info);
    void on_departed(const dts::mdns::service_info& info);
    void ping_loop(std::stop_token stop);

    [[nodiscard]] static CandidateId make_candidate_id(const dts::mdns::service_info& info);
    [[nodiscard]] static HardwareCandidate make_candidate(const dts::mdns::service_info& info);

    dts::mdns::monitor& monitor_;
    InventoryRegistry& registry_;
    dts::scoped_connection arrived_conn_;
    dts::scoped_connection departed_conn_;

    struct PingTarget {
        std::string ip;
        HardwareCandidate candidate;
        int consecutive_failures{0};
        bool gone{false};
    };

    std::mutex ping_mutex_;
    std::unordered_map<std::string, PingTarget> ping_targets_; // keyed by stable_id
    std::jthread ping_thread_;
};

} // namespace auracle::inventory
