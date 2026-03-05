#pragma once

#include "prober.hpp"

namespace auracle::inventory {

class HostBluetoothProber final : public IProber {
public:
    ProbeResult probe(const HardwareCandidate& candidate) override;
};

} // namespace auracle::inventory
