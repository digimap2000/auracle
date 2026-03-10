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
};

struct ObservationEvent {
    std::string unit_id;
    std::variant<BleAdvertisement> payload;
};

} // namespace auracle::observation
