#pragma once

#include "provider.hpp"
#include "../registry.hpp"
#include "../types.hpp"

#include <dts/bluetooth.hpp>
#include <dts/signal.hpp>

namespace auracle::inventory {

class HostBluetoothCandidateProvider final : public ICandidateProvider {
public:
    HostBluetoothCandidateProvider(
        dts::bluetooth::monitor& monitor,
        InventoryRegistry& registry);

    void start() override;
    void stop() override;

private:
    void on_arrived(const dts::bluetooth::adapter_info& info);
    void on_departed(const dts::bluetooth::adapter_info& info);
    void on_changed(const dts::bluetooth::adapter_info& info);

    [[nodiscard]] static CandidateId make_candidate_id(
        const dts::bluetooth::adapter_info& info);
    [[nodiscard]] static HardwareCandidate make_candidate(
        const dts::bluetooth::adapter_info& info);

    dts::bluetooth::monitor& monitor_;
    InventoryRegistry& registry_;
    dts::scoped_connection arrived_conn_;
    dts::scoped_connection departed_conn_;
    dts::scoped_connection changed_conn_;
};

} // namespace auracle::inventory
