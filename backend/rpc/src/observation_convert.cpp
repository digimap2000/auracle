#include "observation_convert.hpp"

#include <assigned-numbers/assigned_numbers.hpp>

#include <chrono>
#include <format>
#include <span>
#include <variant>

namespace auracle::rpc {

namespace {

[[nodiscard]] int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] std::string hex_bytes(std::span<const std::uint8_t> bytes) {
    std::string out = "0x";
    out.reserve(2U + bytes.size() * 2U);
    for (const auto byte : bytes) {
        out += std::format("{:02X}", byte);
    }
    return out;
}

void append_decoded_service_data(
    std::span<const std::uint8_t> bytes,
    obs_proto::DecodeAdvertisementResponse* dst) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto field_len = bytes[offset];
        if (field_len == 0) {
            break;
        }

        if (offset + 1U + field_len > bytes.size()) {
            break;
        }

        const auto field_type = bytes[offset + 1U];
        const auto field = bytes.subspan(offset + 2U, field_len - 1U);

        std::optional<assigned_numbers::decoded_service_data> decoded;
        switch (field_type) {
        case 0x16:
            if (field.size() >= 2U) {
                const auto uuid = static_cast<std::uint16_t>(field[0] | (field[1U] << 8U));
                const auto uuid_str = std::format("{:08x}-0000-1000-8000-00805f9b34fb", uuid);
                decoded = assigned_numbers::decode_service_data(
                    uuid_str, field.subspan(2U));
            }
            break;
        default:
            break;
        }

        if (decoded.has_value()) {
            auto* out = dst->add_decoded_service_data();
            out->set_service_uuid(decoded->service_uuid);
            out->set_service_label(decoded->service_label);
            out->set_raw_value(hex_bytes(decoded->raw_value));
            for (const auto& field_value : decoded->fields) {
                auto* out_field = out->add_fields();
                out_field->set_field(field_value.field);
                out_field->set_type(field_value.type);
                out_field->set_value(field_value.value);
            }
        }

        offset += static_cast<std::size_t>(field_len) + 1U;
    }
}

} // namespace

void to_proto(const observation::BleAdvertisement& src, obs_proto::BleAdvertisement* dst) {
    dst->set_device_id(src.device_id);
    dst->set_name(src.name);
    dst->set_rssi(src.rssi);
    dst->set_tx_power(src.tx_power);
    for (const auto& uuid : src.service_uuids) {
        dst->add_service_uuids(uuid);
        if (const auto name = assigned_numbers::service_name(uuid); name.has_value()) {
            (*dst->mutable_service_labels())[uuid] = std::string(*name);
        }
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

void decode_advertisement_to_proto(
    std::span<const std::uint8_t> raw_data,
    std::span<const std::uint8_t> raw_scan_response,
    obs_proto::DecodeAdvertisementResponse* dst) {
    append_decoded_service_data(raw_data, dst);
    append_decoded_service_data(raw_scan_response, dst);
}

} // namespace auracle::rpc
