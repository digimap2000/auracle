#include "convert.hpp"

#include <chrono>
#include <variant>

namespace auracle::rpc {

namespace {

[[nodiscard]] int64_t to_ms(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()).count();
}

[[nodiscard]] proto::Transport to_proto_transport(inventory::Transport t) {
    switch (t) {
    case inventory::Transport::Serial: return proto::TRANSPORT_SERIAL;
    case inventory::Transport::Mdns:           return proto::TRANSPORT_MDNS;
    case inventory::Transport::HostBluetooth:  return proto::TRANSPORT_HOST_BLUETOOTH;
    }
    return proto::TRANSPORT_UNSPECIFIED;
}

[[nodiscard]] proto::HardwareKind to_proto_kind(inventory::HardwareKind k) {
    switch (k) {
    case inventory::HardwareKind::Unknown:        return proto::HARDWARE_KIND_UNSPECIFIED;
    case inventory::HardwareKind::Nrf5340AudioDK:       return proto::HARDWARE_KIND_NRF5340_AUDIO_DK;
    case inventory::HardwareKind::HostBluetoothAdapter: return proto::HARDWARE_KIND_HOST_BLUETOOTH_ADAPTER;
    }
    return proto::HARDWARE_KIND_UNSPECIFIED;
}

[[nodiscard]] proto::Capability to_proto_capability(inventory::Capability c) {
    switch (c) {
    case inventory::Capability::BleScan:        return proto::CAPABILITY_BLE_SCAN;
    case inventory::Capability::LeAudioSink:    return proto::CAPABILITY_LE_AUDIO_SINK;
    case inventory::Capability::LeAudioSource:  return proto::CAPABILITY_LE_AUDIO_SOURCE;
    case inventory::Capability::UnicastClient:  return proto::CAPABILITY_UNICAST_CLIENT;
    }
    return proto::CAPABILITY_UNSPECIFIED;
}

} // namespace

void to_proto(const inventory::SerialDetails& src, proto::SerialDetails* dst) {
    dst->set_port(src.port);
    dst->set_usb_vid(src.usb_vid);
    dst->set_usb_pid(src.usb_pid);
    dst->set_manufacturer(src.manufacturer);
    dst->set_product(src.product);
    dst->set_usb_serial(src.usb_serial);
}

void to_proto(const inventory::MdnsDetails& src, proto::MdnsDetails* dst) {
    dst->set_service(src.service);
    dst->set_instance(src.instance);
    dst->set_host(src.host);
    dst->set_port(src.port);
}

void to_proto(const inventory::HostBluetoothDetails& src, proto::HostBluetoothDetails* dst) {
    dst->set_adapter_key(src.adapter_key);
    dst->set_name(src.name);
    dst->set_address(src.address);
    dst->set_powered(src.powered);
}

void to_proto(const inventory::Identity& src, proto::Identity* dst) {
    dst->set_vendor(src.vendor);
    dst->set_product(src.product);
    dst->set_serial(src.serial);
    dst->set_firmware_version(src.firmware_version);
}

void to_proto(const inventory::Lease& src, proto::Lease* dst) {
    dst->set_token(src.token.value);
    dst->set_unit_id(src.unit_id.value);
    dst->set_client_id(src.client_id);
    dst->set_purpose(src.purpose);
    dst->set_claimed_at_ms(to_ms(src.claimed_at));
}

void to_proto(const inventory::HardwareCandidate& src, proto::HardwareCandidate* dst) {
    dst->set_id(src.id.value);
    dst->set_transport(to_proto_transport(src.transport));
    dst->set_present(src.present);
    dst->set_first_seen_ms(to_ms(src.first_seen));
    dst->set_last_seen_ms(to_ms(src.last_seen));

    std::visit([dst](const auto& details) {
        using T = std::decay_t<decltype(details)>;
        if constexpr (std::is_same_v<T, inventory::SerialDetails>) {
            to_proto(details, dst->mutable_serial());
        } else if constexpr (std::is_same_v<T, inventory::MdnsDetails>) {
            to_proto(details, dst->mutable_mdns());
        } else if constexpr (std::is_same_v<T, inventory::HostBluetoothDetails>) {
            to_proto(details, dst->mutable_host_bluetooth());
        }
    }, src.details);
}

void to_proto(const inventory::HardwareUnit& src, proto::HardwareUnit* dst) {
    dst->set_id(src.id.value);
    dst->set_kind(to_proto_kind(src.kind));
    dst->set_present(src.present);
    dst->set_bound_candidate(src.bound_candidate.value);
    to_proto(src.identity, dst->mutable_identity());
    if (src.lease.has_value()) {
        to_proto(*src.lease, dst->mutable_lease());
    }
    for (const auto& cap : src.capabilities) {
        dst->add_capabilities(to_proto_capability(cap));
    }
}

void to_proto(const inventory::InventoryEvent& src, proto::InventoryEvent* dst) {
    dst->set_timestamp_ms(to_ms(std::chrono::system_clock::now()));

    std::visit([dst](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, inventory::CandidateAdded>) {
            to_proto(e.candidate, dst->mutable_candidate_added()->mutable_candidate());
        } else if constexpr (std::is_same_v<T, inventory::CandidateUpdated>) {
            to_proto(e.candidate, dst->mutable_candidate_updated()->mutable_candidate());
        } else if constexpr (std::is_same_v<T, inventory::CandidateGone>) {
            dst->mutable_candidate_gone()->set_id(e.id.value);
        } else if constexpr (std::is_same_v<T, inventory::UnitAdded>) {
            to_proto(e.unit, dst->mutable_unit_added()->mutable_unit());
        } else if constexpr (std::is_same_v<T, inventory::UnitUpdated>) {
            to_proto(e.unit, dst->mutable_unit_updated()->mutable_unit());
        } else if constexpr (std::is_same_v<T, inventory::UnitRemoved>) {
            dst->mutable_unit_removed()->set_id(e.id.value);
        } else if constexpr (std::is_same_v<T, inventory::UnitOnline>) {
            dst->mutable_unit_online()->set_id(e.id.value);
        } else if constexpr (std::is_same_v<T, inventory::UnitOffline>) {
            dst->mutable_unit_offline()->set_id(e.id.value);
        } else if constexpr (std::is_same_v<T, inventory::ClaimChanged>) {
            auto* claim = dst->mutable_claim_changed();
            claim->set_id(e.id.value);
            if (e.lease.has_value()) {
                to_proto(*e.lease, claim->mutable_lease());
            }
        }
    }, src);
}

} // namespace auracle::rpc
