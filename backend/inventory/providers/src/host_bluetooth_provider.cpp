#include "../host_bluetooth_provider.hpp"

#include <chrono>
#include <format>

namespace auracle::inventory {

HostBluetoothCandidateProvider::HostBluetoothCandidateProvider(
    dts::bluetooth::monitor& monitor,
    InventoryRegistry& registry)
    : monitor_(monitor)
    , registry_(registry) {}

void HostBluetoothCandidateProvider::start() {
    arrived_conn_ = monitor_.on_arrived_scoped(
        [this](const dts::bluetooth::adapter_info& info) { on_arrived(info); });
    departed_conn_ = monitor_.on_departed_scoped(
        [this](const dts::bluetooth::adapter_info& info) { on_departed(info); });
    changed_conn_ = monitor_.on_changed_scoped(
        [this](const dts::bluetooth::adapter_info& info) { on_changed(info); });
}

void HostBluetoothCandidateProvider::stop() {
    arrived_conn_.reset();
    departed_conn_.reset();
    changed_conn_.reset();
}

void HostBluetoothCandidateProvider::on_arrived(
    const dts::bluetooth::adapter_info& info) {
    registry_.upsert_candidate(make_candidate(info));
}

void HostBluetoothCandidateProvider::on_departed(
    const dts::bluetooth::adapter_info& info) {
    registry_.mark_candidate_gone(make_candidate_id(info));
}

void HostBluetoothCandidateProvider::on_changed(
    const dts::bluetooth::adapter_info& info) {
    registry_.upsert_candidate(make_candidate(info));
}

CandidateId HostBluetoothCandidateProvider::make_candidate_id(
    const dts::bluetooth::adapter_info& info) {
    return CandidateId{std::format("hostbt:{}", info.adapter_key)};
}

HardwareCandidate HostBluetoothCandidateProvider::make_candidate(
    const dts::bluetooth::adapter_info& info) {
    const auto now = std::chrono::system_clock::now();

    HostBluetoothDetails details;
    details.adapter_key = info.adapter_key;
    details.name = info.name;
    details.address = info.address;
    details.powered = info.powered;

    return HardwareCandidate{
        .id = make_candidate_id(info),
        .transport = Transport::HostBluetooth,
        .present = info.present,
        .first_seen = now,
        .last_seen = now,
        .details = std::move(details),
    };
}

} // namespace auracle::inventory
