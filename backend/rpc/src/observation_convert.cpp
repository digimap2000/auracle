#include "observation_convert.hpp"

#include <chrono>
#include <variant>

namespace auracle::rpc {

namespace {

[[nodiscard]] int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

void to_proto(const observation::BleAdvertisement& src, obs_proto::BleAdvertisement* dst) {
    dst->set_device_id(src.device_id);
    dst->set_name(src.name);
    dst->set_rssi(src.rssi);
    dst->set_tx_power(src.tx_power);
    for (const auto& uuid : src.service_uuids) {
        dst->add_service_uuids(uuid);
    }
    if (!src.manufacturer_data.empty()) {
        dst->set_manufacturer_data(
            src.manufacturer_data.data(), src.manufacturer_data.size());
    }
    dst->set_company_id(src.company_id);
    dst->set_address_type(src.address_type);
    dst->set_sid(src.sid);
    dst->set_adv_type(src.adv_type);
    dst->set_adv_props(src.adv_props);
    dst->set_interval(src.interval);
    dst->set_primary_phy(src.primary_phy);
    dst->set_secondary_phy(src.secondary_phy);
    if (!src.raw_data.empty()) {
        dst->set_raw_data(src.raw_data.data(), src.raw_data.size());
    }
    if (!src.raw_scan_response.empty()) {
        dst->set_raw_scan_response(src.raw_scan_response.data(), src.raw_scan_response.size());
    }
}

void to_proto(const observation::ObservedBleDevice& src, obs_proto::ObservedBleDevice* dst) {
    dst->set_unit_id(src.unit_id);
    to_proto(src.advertisement, dst->mutable_advertisement());
    dst->set_first_seen_ms(src.first_seen_ms);
    dst->set_last_seen_ms(src.last_seen_ms);
    dst->set_seen_count(src.seen_count);
}

void to_proto(const observation::ObservationEvent& src, obs_proto::ObservationEvent* dst) {
    dst->set_timestamp_ms(now_ms());
    dst->set_unit_id(src.unit_id);

    std::visit([dst](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, observation::BleAdvertisement>) {
            to_proto(payload, dst->mutable_ble_advertisement());
        }
    }, src.payload);
}

} // namespace auracle::rpc
