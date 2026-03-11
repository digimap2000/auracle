#pragma once

#include <compliance/facts.hpp>

#include <observation/types.hpp>

#include <span>

namespace auracle::compliance {

[[nodiscard]] ComplianceFacts normalize_ea_facts(std::span<const std::uint8_t> raw_data);
[[nodiscard]] ComplianceFacts normalize_ea_facts(const observation::BleAdvertisement& advertisement);

} // namespace auracle::compliance
