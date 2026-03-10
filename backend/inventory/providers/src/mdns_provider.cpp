#include "../mdns_provider.hpp"

#include <chrono>
#include <format>

namespace auracle::inventory {

MdnsCandidateProvider::MdnsCandidateProvider(
    dts::mdns::monitor& monitor,
    InventoryRegistry& registry)
    : monitor_(monitor)
    , registry_(registry) {}

void MdnsCandidateProvider::start() {
    arrived_conn_ = monitor_.on_arrived_scoped(
        [this](const dts::mdns::service_info& info) { on_arrived(info); });
    departed_conn_ = monitor_.on_departed_scoped(
        [this](const dts::mdns::service_info& info) { on_departed(info); });
}

void MdnsCandidateProvider::stop() {
    arrived_conn_.reset();
    departed_conn_.reset();
}

void MdnsCandidateProvider::on_arrived(const dts::mdns::service_info& info) {
    registry_.upsert_candidate(make_candidate(info));
}

void MdnsCandidateProvider::on_departed(const dts::mdns::service_info& info) {
    registry_.mark_candidate_gone(make_candidate_id(info));
}

CandidateId MdnsCandidateProvider::make_candidate_id(const dts::mdns::service_info& info) {
    return CandidateId{std::format("mdns:{}", info.stable_id)};
}

HardwareCandidate MdnsCandidateProvider::make_candidate(const dts::mdns::service_info& info) {
    const auto now = std::chrono::system_clock::now();

    MdnsDetails details;
    details.service = info.service_type;
    details.instance = info.instance;
    details.host = info.host;
    details.port = info.port;

    return HardwareCandidate{
        .id = make_candidate_id(info),
        .transport = Transport::Mdns,
        .present = true,
        .first_seen = now,
        .last_seen = now,
        .details = std::move(details),
    };
}

} // namespace auracle::inventory
