#pragma once

#include "prober.hpp"

namespace auracle::inventory {

class MdnsAgentProber final : public IProber {
public:
    ProbeResult probe(const HardwareCandidate& candidate) override;
};

} // namespace auracle::inventory
