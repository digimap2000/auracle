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
