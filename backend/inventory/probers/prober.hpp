#pragma once

#include "../candidate.hpp"
#include "../unit.hpp"

namespace auracle::inventory {

struct IProber {
    virtual ~IProber() = default;
    virtual ProbeResult probe(const HardwareCandidate& candidate) = 0;
};

} // namespace auracle::inventory
