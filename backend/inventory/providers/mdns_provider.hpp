#pragma once

#include "provider.hpp"
#include "../candidate.hpp"
#include "../registry.hpp"
#include "../types.hpp"

#include <dts/mdns.hpp>
#include <dts/signal.hpp>

namespace auracle::inventory {

class MdnsCandidateProvider final : public ICandidateProvider {
public:
    MdnsCandidateProvider(dts::mdns::monitor& monitor, InventoryRegistry& registry);

    void start() override;
    void stop() override;

private:
    void on_arrived(const dts::mdns::service_info& info);
    void on_departed(const dts::mdns::service_info& info);

    [[nodiscard]] static CandidateId make_candidate_id(const dts::mdns::service_info& info);
    [[nodiscard]] static HardwareCandidate make_candidate(const dts::mdns::service_info& info);

    dts::mdns::monitor& monitor_;
    InventoryRegistry& registry_;
    dts::scoped_connection arrived_conn_;
    dts::scoped_connection departed_conn_;
};

} // namespace auracle::inventory
