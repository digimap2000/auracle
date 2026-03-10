#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

namespace auracle::inventory {

// ---------------------------------------------------------------------------
// Strong identifiers
// ---------------------------------------------------------------------------

struct CandidateId {
    std::string value;
    bool operator==(const CandidateId&) const = default;
};

struct UnitId {
    std::string value;
    bool operator==(const UnitId&) const = default;
};

struct LeaseToken {
    std::string value;
    bool operator==(const LeaseToken&) const = default;
};

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class Transport {
    Serial,
    Mdns,
    HostBluetooth,
};

enum class HardwareKind {
    Unknown,
    Nrf5340AudioDK,
    HostBluetoothAdapter,
};

enum class ProbeOutcome {
    Unsupported,
    Supported,
    TransientFailure,
};

enum class Capability {
    BleScan,
    LeAudioSink,
    LeAudioSource,
    UnicastClient,
};

// ---------------------------------------------------------------------------
// Transport-specific candidate details
// ---------------------------------------------------------------------------

struct SerialDetails {
    std::string port;
    std::string usb_vid;
    std::string usb_pid;
    std::string manufacturer;
    std::string product;
    std::string usb_serial;
};

struct MdnsDetails {
    std::string service;
    std::string instance;
    std::string host;
    std::uint16_t port{0};
};

struct HostBluetoothDetails {
    std::string adapter_key;
    std::string name;
    std::string address;
    bool powered{false};
};

using CandidateDetails = std::variant<SerialDetails, MdnsDetails, HostBluetoothDetails>;

} // namespace auracle::inventory

// Hash specializations for strong identifiers

template <>
struct std::hash<auracle::inventory::CandidateId> {
    [[nodiscard]] std::size_t operator()(const auracle::inventory::CandidateId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};

template <>
struct std::hash<auracle::inventory::UnitId> {
    [[nodiscard]] std::size_t operator()(const auracle::inventory::UnitId& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};

template <>
struct std::hash<auracle::inventory::LeaseToken> {
    [[nodiscard]] std::size_t operator()(const auracle::inventory::LeaseToken& id) const noexcept {
        return std::hash<std::string>{}(id.value);
    }
};
