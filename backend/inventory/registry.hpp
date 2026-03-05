#pragma once

#include "candidate.hpp"
#include "events.hpp"
#include "types.hpp"
#include "unit.hpp"

#include <dts/signal.hpp>

#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace auracle::inventory {

class InventoryRegistry {
public:
    // Providers call these
    void upsert_candidate(const HardwareCandidate& candidate);
    void mark_candidate_gone(const CandidateId& id);

    // Probers call this (future)
    void submit_probe_result(const CandidateId& id, const ProbeResult& result);

    // Snapshot queries
    [[nodiscard]] std::vector<HardwareCandidate> list_candidates(bool include_gone = false) const;
    [[nodiscard]] std::vector<HardwareUnit> list_units(bool include_offline = false) const;

    // Claiming (future)
    [[nodiscard]] std::expected<Lease, std::string>
    claim_unit(const UnitId& id, std::string client_id, std::string purpose);

    [[nodiscard]] std::expected<void, std::string>
    release_unit(const LeaseToken& token);

    // Event stream
    dts::signal<void(const InventoryEvent&)> on_event;

private:
    mutable std::mutex mutex_;
    std::unordered_map<CandidateId, HardwareCandidate> candidates_;
    std::unordered_map<UnitId, HardwareUnit> units_;
};

} // namespace auracle::inventory
