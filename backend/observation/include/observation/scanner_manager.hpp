#pragma once

#include "types.hpp"
#include "scanner.hpp"

#include <inventory/inventory.hpp>
#include <dts/signal.hpp>

#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace auracle::observation {

// Manages BLE scanners bound to inventory units.
// When a HostBluetoothAdapter unit with BLE_SCAN capability is promoted,
// the daemon binds a scanner to it. StartScan/StopScan control the scanner
// through this manager.
class ScannerManager {
public:
    explicit ScannerManager(inventory::InventoryRegistry& registry);

    // Bind a scanner to a unit. Called by daemon when a capable unit appears.
    void bind_scanner(const std::string& unit_id,
                      ScannerPtr scanner);

    // Remove scanner binding. Called when unit is removed.
    void unbind_scanner(const std::string& unit_id);

    // Start scanning on a unit's bound scanner.
    [[nodiscard]] std::expected<void, std::string>
    start_scan(const std::string& unit_id, bool allow_duplicates);

    // Stop scanning on a unit's bound scanner.
    [[nodiscard]] std::expected<void, std::string>
    stop_scan(const std::string& unit_id);

    // Return the daemon's retained BLE device view for one unit or all units.
    [[nodiscard]] std::vector<ObservedBleDevice>
    list_observed_devices(const std::string& unit_id = {}) const;

    // Signal emitted for every observation from any active scanner.
    dts::signal<void(const ObservationEvent&)> on_observation;

private:
    struct BoundScanner {
        ScannerPtr scanner;
        std::unordered_map<std::string, ObservedBleDevice> devices;
    };

    inventory::InventoryRegistry& registry_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, BoundScanner> scanners_;
};

} // namespace auracle::observation
