#include <observation/scanner_manager.hpp>

#include <dts/log.hpp>

namespace auracle::observation {

namespace {
constexpr std::string_view log_tag = "scanner-mgr";
} // namespace

ScannerManager::ScannerManager(inventory::InventoryRegistry& registry)
    : registry_(registry) {}

void ScannerManager::bind_scanner(const std::string& unit_id,
                                  std::unique_ptr<dts::bluetooth::scanner> scanner) {
    std::scoped_lock lock(mutex_);

    auto [it, inserted] = scanners_.try_emplace(unit_id, BoundScanner{});
    if (!inserted) {
        dts::log::warn(log_tag, "scanner already bound to unit {}", unit_id);
        return;
    }

    it->second.scanner = std::move(scanner);
    dts::log::info(log_tag, "bound scanner to unit {}", unit_id);
}

void ScannerManager::unbind_scanner(const std::string& unit_id) {
    std::scoped_lock lock(mutex_);
    auto it = scanners_.find(unit_id);
    if (it == scanners_.end()) {
        return;
    }

    // Stop scanning if active — scanner destructor handles this too
    if (it->second.scanner && it->second.scanner->scanning()) {
        it->second.scanner->stop();
    }

    scanners_.erase(it);
    dts::log::info(log_tag, "unbound scanner from unit {}", unit_id);
}

std::expected<void, std::string>
ScannerManager::start_scan(const std::string& unit_id, bool allow_duplicates) {
    std::scoped_lock lock(mutex_);
    auto it = scanners_.find(unit_id);
    if (it == scanners_.end()) {
        return std::unexpected("no scanner bound to unit " + unit_id);
    }

    auto& bound = it->second;
    if (bound.scanner->scanning()) {
        return {};  // already scanning
    }

    // Subscribe to advertisements before starting
    bound.adv_conn = bound.scanner->on_advertisement_scoped(
        [this, uid = unit_id](const dts::bluetooth::advertisement& adv) {
            ObservationEvent event{
                .unit_id = uid,
                .payload = BleAdvertisement{
                    .device_id = adv.device_id,
                    .name = adv.name,
                    .rssi = adv.rssi,
                    .tx_power = adv.tx_power,
                    .service_uuids = adv.service_uuids,
                    .manufacturer_data = adv.manufacturer_data,
                    .company_id = adv.company_id,
                },
            };
            on_observation.emit(event);
        });

    bound.scanner->start(dts::bluetooth::scan_options{
        .allow_duplicates = allow_duplicates,
    });

    dts::log::info(log_tag, "started scan on unit {}", unit_id);
    return {};
}

std::expected<void, std::string>
ScannerManager::stop_scan(const std::string& unit_id) {
    std::scoped_lock lock(mutex_);
    auto it = scanners_.find(unit_id);
    if (it == scanners_.end()) {
        return std::unexpected("no scanner bound to unit " + unit_id);
    }

    auto& bound = it->second;
    bound.adv_conn.reset();
    bound.scanner->stop();

    dts::log::info(log_tag, "stopped scan on unit {}", unit_id);
    return {};
}

} // namespace auracle::observation
