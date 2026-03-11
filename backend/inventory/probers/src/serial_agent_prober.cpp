#include "../serial_agent_prober.hpp"

namespace auracle::inventory {

namespace {

[[nodiscard]] bool is_nordic_audio_dk(const SerialDetails& details) {
    return details.usb_vid == "1915" && details.usb_pid == "5340";
}

} // namespace

ProbeResult SerialAgentProber::probe(const HardwareCandidate& candidate) {
    if (candidate.transport != Transport::Serial) {
        return {.outcome = ProbeOutcome::Unsupported};
    }

    const auto* details = std::get_if<SerialDetails>(&candidate.details);
    if (details == nullptr) {
        return {.outcome = ProbeOutcome::Unsupported};
    }

    if (!is_nordic_audio_dk(*details)) {
        return {.outcome = ProbeOutcome::TransientFailure,
                .kind = HardwareKind::Unknown};
    }

    const std::string serial =
        !details->usb_serial.empty() ? details->usb_serial : details->port;

    return {
        .outcome = ProbeOutcome::Supported,
        .kind = HardwareKind::Nrf5340AudioDK,
        .identity = {
            .vendor = "Nordic Semiconductor",
            .product = !details->product.empty() ? details->product : "nRF5340 Audio DK",
            .serial = serial,
        },
        .present = true,
        .capabilities = {
            Capability::BleScan,
            Capability::LeAudioSink,
            Capability::LeAudioSource,
            Capability::UnicastClient,
        },
    };
}

} // namespace auracle::inventory
