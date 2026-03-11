#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>

namespace auracle::compliance {

struct EaFacts {
    std::unordered_set<std::uint16_t> service_data_uuids;
    std::optional<std::uint16_t> appearance;
};

struct ComplianceFacts {
    EaFacts ea;
};

} // namespace auracle::compliance
