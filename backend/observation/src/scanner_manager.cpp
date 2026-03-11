#include <observation/scanner_manager.hpp>

#include <dts/log.hpp>

#include <chrono>

namespace auracle::observation {

namespace {
constexpr std::string_view log_tag = "scanner-mgr";

[[nodiscard]] std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] BleAdvertisement merge_advertisement(
    const BleAdvertisement& existing,
    BleAdvertisement incoming) {

    if (incoming.name.empty() && !existing.name.empty()) {
        incoming.name = existing.name;
    }

    if (incoming.tx_power == 127 && existing.tx_power != 127) {
        incoming.tx_power = existing.tx_power;
    }

    if (incoming.service_uuids.empty() && !existing.service_uuids.empty()) {
        incoming.service_uuids = existing.service_uuids;
    }

    if (incoming.manufacturer_data.empty() && !existing.manufacturer_data.empty()) {
        incoming.manufacturer_data = existing.manufacturer_data;
    }

    if (incoming.company_id == 0 && existing.company_id != 0) {
        incoming.company_id = existing.company_id;
    }

    return incoming;
}
} // namespace

ScannerManager::ScannerManager(inventory::InventoryRegistry& registry)
    : registry_(registry) {}

void ScannerManager::bind_scanner(const std::string& unit_id,
                                  ScannerPtr scanner) {
    std::scoped_lock lock(mutex_);

    auto [it, inserted] = scanners_.try_emplace(unit_id, BoundScanner{});
    if (!inserted) {
        dts::log::debug(log_tag, "scanner already bound to unit {}, keeping existing binding", unit_id);
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
        auto _ = it->second.scanner->stop();
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
    if (bound.scanner == nullptr) {
        return std::unexpected("no scanner implementation bound to unit " + unit_id);
    }

    if (bound.scanner->scanning()) {
        return {};  // already scanning
    }

    bound.scanner->set_advertisement_handler([this, uid = unit_id](const BleAdvertisement& advertisement) {
        const auto timestamp = now_ms();
        ObservationEvent event;

        {
            std::scoped_lock callback_lock(mutex_);
            auto scanner_it = scanners_.find(uid);
            if (scanner_it == scanners_.end()) {
                return;
            }

            auto incoming = advertisement;

            auto& devices = scanner_it->second.devices;
            auto [device_it, inserted] = devices.try_emplace(
                incoming.device_id,
                ObservedBleDevice{
                    .unit_id = uid,
                    .advertisement = incoming,
                    .first_seen_ms = timestamp,
                    .last_seen_ms = timestamp,
                    .seen_count = 0,
                });

            auto& observed = device_it->second;
            if (!inserted) {
                observed.advertisement = merge_advertisement(observed.advertisement, std::move(incoming));
                observed.last_seen_ms = timestamp;
            }

            observed.seen_count += 1;

            event = ObservationEvent{
                .unit_id = uid,
                .payload = observed.advertisement,
            };
        }

        on_observation.emit(event);
    });

    auto result = bound.scanner->start(allow_duplicates);
    if (!result) {
        return result;
    }

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
    if (bound.scanner == nullptr) {
        return std::unexpected("no scanner implementation bound to unit " + unit_id);
    }

    auto result = bound.scanner->stop();
    if (!result) {
        return result;
    }

    dts::log::info(log_tag, "stopped scan on unit {}", unit_id);
    return {};
}

std::vector<ObservedBleDevice>
ScannerManager::list_observed_devices(const std::string& unit_id) const {
    std::scoped_lock lock(mutex_);

    std::vector<ObservedBleDevice> result;

    auto append_devices = [&result](const BoundScanner& bound) {
        result.reserve(result.size() + bound.devices.size());
        for (const auto& [_, device] : bound.devices) {
            result.push_back(device);
        }
    };

    if (!unit_id.empty()) {
        auto it = scanners_.find(unit_id);
        if (it != scanners_.end()) {
            append_devices(it->second);
        }
        return result;
    }

    for (const auto& [_, bound] : scanners_) {
        append_devices(bound);
    }

    return result;
}

} // namespace auracle::observation
