#pragma once

#include "types.hpp"

#include <chrono>

namespace auracle::inventory {

struct HardwareCandidate {
    CandidateId id;
    Transport transport{};
    bool present{true};
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    CandidateDetails details;
};

} // namespace auracle::inventory
