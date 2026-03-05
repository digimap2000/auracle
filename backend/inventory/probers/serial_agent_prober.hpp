#pragma once

#include "prober.hpp"

namespace auracle::inventory {

// Future: opens serial port, sends HELLO command, parses response.
class SerialAgentProber final : public IProber {
public:
    ProbeResult probe(const HardwareCandidate& candidate) override;
};

} // namespace auracle::inventory
