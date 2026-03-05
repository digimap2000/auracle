#include "../serial_provider.hpp"

#include <chrono>
#include <format>

namespace auracle::inventory {

SerialCandidateProvider::SerialCandidateProvider(
    dts::serial::monitor& monitor,
    InventoryRegistry& registry)
    : monitor_(monitor)
    , registry_(registry) {}

void SerialCandidateProvider::start() {
    arrived_conn_ = monitor_.on_arrived_scoped(
        [this](const dts::serial::port_info& info) { on_arrived(info); });
    departed_conn_ = monitor_.on_departed_scoped(
        [this](const dts::serial::port_info& info) { on_departed(info); });
}

void SerialCandidateProvider::stop() {
    arrived_conn_.reset();
    departed_conn_.reset();
}

void SerialCandidateProvider::on_arrived(const dts::serial::port_info& info) {
    registry_.upsert_candidate(make_candidate(info));
}

void SerialCandidateProvider::on_departed(const dts::serial::port_info& info) {
    registry_.mark_candidate_gone(make_candidate_id(info));
}

CandidateId SerialCandidateProvider::make_candidate_id(const dts::serial::port_info& info) {
    if (info.usb.has_value()
        && info.usb->serial_number.has_value()
        && !info.usb->serial_number->empty()) {
        return CandidateId{std::format("serial:usb={}", *info.usb->serial_number)};
    }
    return CandidateId{std::format("serial:port={}", info.path)};
}

HardwareCandidate SerialCandidateProvider::make_candidate(const dts::serial::port_info& info) {
    const auto now = std::chrono::system_clock::now();

    SerialDetails details;
    details.port = info.path;
    if (info.usb.has_value()) {
        if (info.usb->vendor_id.has_value()) {
            details.usb_vid = std::format("{:04x}", *info.usb->vendor_id);
        }
        if (info.usb->product_id.has_value()) {
            details.usb_pid = std::format("{:04x}", *info.usb->product_id);
        }
        if (info.usb->serial_number.has_value()) {
            details.usb_serial = *info.usb->serial_number;
        }
    }
    details.product = info.label;

    return HardwareCandidate{
        .id = make_candidate_id(info),
        .transport = Transport::Serial,
        .present = true,
        .first_seen = now,
        .last_seen = now,
        .details = std::move(details),
    };
}

} // namespace auracle::inventory
