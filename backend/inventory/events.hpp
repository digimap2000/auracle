#pragma once

#include "candidate.hpp"
#include "types.hpp"
#include "unit.hpp"

#include <optional>
#include <variant>

namespace auracle::inventory {

struct CandidateAdded {
    HardwareCandidate candidate;
};

struct CandidateUpdated {
    HardwareCandidate candidate;
};

struct CandidateGone {
    CandidateId id;
};

struct UnitAdded {
    HardwareUnit unit;
};

struct UnitUpdated {
    HardwareUnit unit;
};

struct UnitRemoved {
    UnitId id;
};

struct UnitOnline {
    UnitId id;
};

struct UnitOffline {
    UnitId id;
};

struct ClaimChanged {
    UnitId id;
    std::optional<Lease> lease;
};

using InventoryEvent = std::variant<
    CandidateAdded,
    CandidateUpdated,
    CandidateGone,
    UnitAdded,
    UnitUpdated,
    UnitRemoved,
    UnitOnline,
    UnitOffline,
    ClaimChanged
>;

} // namespace auracle::inventory
