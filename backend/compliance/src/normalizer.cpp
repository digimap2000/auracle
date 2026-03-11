#include <compliance/normalizer.hpp>

namespace auracle::compliance {
namespace {

void collect_service_data_uuids(
    std::span<const std::uint8_t> bytes,
    EaFacts& ea_facts) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::uint8_t field_length = bytes[offset];
        if (field_length == 0) {
            break;
        }

        const std::size_t next = offset + static_cast<std::size_t>(field_length) + 1U;
        if (next > bytes.size()) {
            break;
        }

        if (field_length >= 3U) {
            const std::uint8_t ad_type = bytes[offset + 1U];
            if (ad_type == 0x16) {
                const std::uint16_t uuid = static_cast<std::uint16_t>(
                    bytes[offset + 2U] | (bytes[offset + 3U] << 8U));
                ea_facts.service_data_uuids.insert(uuid);
            }
        }

        offset = next;
    }
}

} // namespace

ComplianceFacts normalize_ea_facts(std::span<const std::uint8_t> raw_data) {
    ComplianceFacts facts;
    collect_service_data_uuids(raw_data, facts.ea);
    return facts;
}

ComplianceFacts normalize_ea_facts(const observation::BleAdvertisement& advertisement) {
    return normalize_ea_facts(advertisement.raw_data);
}

} // namespace auracle::compliance
