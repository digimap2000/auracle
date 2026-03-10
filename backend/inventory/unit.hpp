#pragma once

#include "types.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace auracle::inventory {

struct Identity {
    std::string vendor;
    std::string product;
    std::string serial;
    std::string firmware_version;
};

struct ProbeResult {
    ProbeOutcome outcome{};
    HardwareKind kind{};
    Identity identity;
    bool present{true};
    std::vector<Capability> capabilities;
};

struct Lease {
    LeaseToken token;
    UnitId unit_id;
    std::string client_id;
    std::string purpose;
    std::chrono::system_clock::time_point claimed_at;
};

struct HardwareUnit {
    UnitId id;
    HardwareKind kind{};
    bool present{true};
    CandidateId bound_candidate;
    Identity identity;
    std::optional<Lease> lease;
    std::vector<Capability> capabilities;
};

} // namespace auracle::inventory
