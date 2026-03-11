#include <observation/local_bluetooth_scanner.hpp>

namespace auracle::observation {

LocalBluetoothScanner::LocalBluetoothScanner(std::unique_ptr<dts::bluetooth::scanner> scanner)
    : scanner_(std::move(scanner)) {}

void LocalBluetoothScanner::set_advertisement_handler(AdvertisementHandler handler) {
    std::scoped_lock lock(mutex_);
    handler_ = std::move(handler);
}

std::expected<void, std::string> LocalBluetoothScanner::start(bool allow_duplicates) {
    std::scoped_lock lock(mutex_);
    if (scanner_ == nullptr) {
        return std::unexpected("local bluetooth scanner not configured");
    }

    if (scanner_->scanning()) {
        return {};
    }

    adv_conn_ = scanner_->on_advertisement_scoped([this](const dts::bluetooth::advertisement& adv) {
        AdvertisementHandler handler;
        {
            std::scoped_lock handler_lock(mutex_);
            handler = handler_;
        }

        if (!handler) {
            return;
        }

        handler(BleAdvertisement{
            .device_id = adv.device_id,
            .name = adv.name,
            .rssi = adv.rssi,
            .tx_power = adv.tx_power,
            .service_uuids = adv.service_uuids,
            .manufacturer_data = adv.manufacturer_data,
            .company_id = adv.company_id,
        });
    });

    scanner_->start(dts::bluetooth::scan_options{
        .allow_duplicates = allow_duplicates,
    });

    return {};
}

std::expected<void, std::string> LocalBluetoothScanner::stop() {
    std::scoped_lock lock(mutex_);
    if (scanner_ == nullptr) {
        return std::unexpected("local bluetooth scanner not configured");
    }

    adv_conn_.reset();
    scanner_->stop();
    return {};
}

bool LocalBluetoothScanner::scanning() const noexcept {
    std::scoped_lock lock(mutex_);
    return scanner_ != nullptr && scanner_->scanning();
}

} // namespace auracle::observation
