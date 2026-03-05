#include "../serial_agent_prober.hpp"

namespace auracle::inventory {

ProbeResult SerialAgentProber::probe(const HardwareCandidate& /*candidate*/) {
    return ProbeResult{
        .outcome = ProbeOutcome::TransientFailure,
        .kind = HardwareKind::Unknown,
        .identity = {},
    };
}

} // namespace auracle::inventory
