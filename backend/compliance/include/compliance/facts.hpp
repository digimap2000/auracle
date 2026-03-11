#pragma once

#include <cstdint>
#include <unordered_set>

namespace auracle::compliance {

struct EaFacts {
    std::unordered_set<std::uint16_t> service_data_uuids;
};

struct ComplianceFacts {
    EaFacts ea;
};

} // namespace auracle::compliance
