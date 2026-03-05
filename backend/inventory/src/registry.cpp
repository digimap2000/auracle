#include "registry.hpp"

#include <chrono>
#include <ranges>

namespace auracle::inventory {

void InventoryRegistry::upsert_candidate(const HardwareCandidate& candidate) {
    InventoryEvent event;

    {
        std::scoped_lock lock(mutex_);
        const auto now = std::chrono::system_clock::now();
        auto it = candidates_.find(candidate.id);

        if (it == candidates_.end()) {
            auto stored = candidate;
            stored.present = true;
            stored.first_seen = now;
            stored.last_seen = now;
            candidates_.emplace(candidate.id, stored);
            event = CandidateAdded{stored};
        } else {
            auto& existing = it->second;
            const bool was_gone = !existing.present;
            existing.details = candidate.details;
            existing.transport = candidate.transport;
            existing.last_seen = now;
            existing.present = true;
            event = was_gone ? InventoryEvent{CandidateAdded{existing}}
                             : InventoryEvent{CandidateUpdated{existing}};
        }
    }

    on_event.emit(event);
}

void InventoryRegistry::mark_candidate_gone(const CandidateId& id) {
    {
        std::scoped_lock lock(mutex_);
        auto it = candidates_.find(id);
        if (it == candidates_.end()) {
            return;
        }
        it->second.present = false;
    }

    on_event.emit(CandidateGone{id});
}

void InventoryRegistry::submit_probe_result(
    const CandidateId& id, const ProbeResult& result) {
    if (result.outcome != ProbeOutcome::Supported) {
        return;
    }

    InventoryEvent event;

    {
        std::scoped_lock lock(mutex_);

        auto cand_it = candidates_.find(id);
        if (cand_it == candidates_.end()) {
            return;
        }

        UnitId unit_id{id.value};
        auto unit_it = units_.find(unit_id);

        if (unit_it == units_.end()) {
            HardwareUnit unit{
                .id = unit_id,
                .kind = result.kind,
                .present = result.present,
                .bound_candidate = id,
                .identity = result.identity,
                .lease = std::nullopt,
            };
            units_.emplace(unit_id, unit);
            event = UnitAdded{unit};
        } else {
            auto& existing = unit_it->second;
            const bool was_present = existing.present;

            existing.kind = result.kind;
            existing.identity = result.identity;
            existing.present = result.present;

            if (!was_present && result.present) {
                event = UnitOnline{unit_id};
            } else if (was_present && !result.present) {
                event = UnitOffline{unit_id};
            } else {
                event = UnitUpdated{existing};
            }
        }
    }

    on_event.emit(event);
}

std::vector<HardwareCandidate> InventoryRegistry::list_candidates(bool include_gone) const {
    std::scoped_lock lock(mutex_);
    std::vector<HardwareCandidate> result;
    for (const auto& candidate : candidates_ | std::views::values) {
        if (include_gone || candidate.present) {
            result.push_back(candidate);
        }
    }
    return result;
}

std::vector<HardwareUnit> InventoryRegistry::list_units(bool include_offline) const {
    std::scoped_lock lock(mutex_);
    std::vector<HardwareUnit> result;
    for (const auto& unit : units_ | std::views::values) {
        if (include_offline || unit.present) {
            result.push_back(unit);
        }
    }
    return result;
}

std::expected<Lease, std::string>
InventoryRegistry::claim_unit(
    const UnitId& /*id*/, std::string /*client_id*/, std::string /*purpose*/) {
    return std::unexpected<std::string>("claiming not yet implemented");
}

std::expected<void, std::string>
InventoryRegistry::release_unit(const LeaseToken& /*token*/) {
    return std::unexpected<std::string>("claiming not yet implemented");
}

} // namespace auracle::inventory
