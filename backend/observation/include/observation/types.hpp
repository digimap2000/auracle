#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace auracle::observation {

struct BleAdvertisement {
    std::string device_id;
    std::string name;
    std::int32_t rssi{0};
    std::int32_t tx_power{127};
    std::vector<std::string> service_uuids;
    std::vector<std::uint8_t> manufacturer_data;
    std::uint16_t company_id{0};
    std::string address_type;
    std::uint32_t sid{255};
    std::uint32_t adv_type{0};
    std::uint32_t adv_props{0};
    std::uint32_t interval{0};
    std::uint32_t primary_phy{0};
    std::uint32_t secondary_phy{0};
    std::vector<std::uint8_t> raw_data;
    std::vector<std::uint8_t> raw_scan_response;
};

struct ObservationEvent {
    std::string unit_id;
    std::variant<BleAdvertisement> payload;
};

struct ObservedBleDevice {
    std::string unit_id;
    BleAdvertisement advertisement;
    std::int64_t first_seen_ms{0};
    std::int64_t last_seen_ms{0};
    std::uint64_t seen_count{0};
};

} // namespace auracle::observation
