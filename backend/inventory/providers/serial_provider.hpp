#pragma once

#include "provider.hpp"
#include "../candidate.hpp"
#include "../registry.hpp"
#include "../types.hpp"

#include <dts/serial.hpp>
#include <dts/signal.hpp>

namespace auracle::inventory {

class SerialCandidateProvider final : public ICandidateProvider {
public:
    SerialCandidateProvider(dts::serial::monitor& monitor, InventoryRegistry& registry);

    void start() override;
    void stop() override;

private:
    void on_arrived(const dts::serial::port_info& info);
    void on_departed(const dts::serial::port_info& info);

    [[nodiscard]] static CandidateId make_candidate_id(const dts::serial::port_info& info);
    [[nodiscard]] static HardwareCandidate make_candidate(const dts::serial::port_info& info);

    dts::serial::monitor& monitor_;
    InventoryRegistry& registry_;
    dts::scoped_connection arrived_conn_;
    dts::scoped_connection departed_conn_;
};

} // namespace auracle::inventory
