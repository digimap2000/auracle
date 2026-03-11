#pragma once

#include "scanner.hpp"

#include <dts/bluetooth.hpp>
#include <dts/signal.hpp>

#include <memory>
#include <mutex>

namespace auracle::observation {

class LocalBluetoothScanner final : public IScanner {
public:
    explicit LocalBluetoothScanner(std::unique_ptr<dts::bluetooth::scanner> scanner);

    void set_advertisement_handler(AdvertisementHandler handler) override;
    [[nodiscard]] std::expected<void, std::string> start(bool allow_duplicates) override;
    [[nodiscard]] std::expected<void, std::string> stop() override;
    [[nodiscard]] bool scanning() const noexcept override;

private:
    std::unique_ptr<dts::bluetooth::scanner> scanner_;
    mutable std::mutex mutex_;
    AdvertisementHandler handler_;
    dts::scoped_connection adv_conn_;
};

} // namespace auracle::observation
