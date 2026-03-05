#include "../host_bluetooth_prober.hpp"

#include <dts/bluetooth.hpp>

#include <algorithm>
#include <ranges>
#include <variant>

namespace auracle::inventory {

ProbeResult HostBluetoothProber::probe(const HardwareCandidate& candidate) {
    if (candidate.transport != Transport::HostBluetooth) {
        return {.outcome = ProbeOutcome::Unsupported};
    }

    const auto* bt_details = std::get_if<HostBluetoothDetails>(&candidate.details);
    if (bt_details == nullptr) {
        return {.outcome = ProbeOutcome::Unsupported};
    }

    std::vector<dts::bluetooth::adapter_info> adapters;
    try {
        adapters = dts::bluetooth::enumerate_adapters();
    } catch (...) {
        return {.outcome = ProbeOutcome::TransientFailure,
                .kind = HardwareKind::HostBluetoothAdapter};
    }

    auto it = std::ranges::find_if(adapters, [&](const auto& a) {
        return a.adapter_key == bt_details->adapter_key;
    });

    if (it == adapters.end()) {
        return {.outcome = ProbeOutcome::TransientFailure,
                .kind = HardwareKind::HostBluetoothAdapter};
    }

    return {
        .outcome = ProbeOutcome::Supported,
        .kind = HardwareKind::HostBluetoothAdapter,
        .identity = {
            .vendor = "Host",
            .product = it->name,
            .serial = it->address,
            .firmware_version = {},
        },
        .present = it->powered,
    };
}

} // namespace auracle::inventory
