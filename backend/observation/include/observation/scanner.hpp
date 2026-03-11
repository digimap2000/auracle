#pragma once

#include "types.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace auracle::observation {

class IScanner {
public:
    using AdvertisementHandler = std::function<void(const BleAdvertisement&)>;

    virtual ~IScanner() = default;

    virtual void set_advertisement_handler(AdvertisementHandler handler) = 0;
    [[nodiscard]] virtual std::expected<void, std::string> start(bool allow_duplicates) = 0;
    [[nodiscard]] virtual std::expected<void, std::string> stop() = 0;
    [[nodiscard]] virtual bool scanning() const noexcept = 0;
};

using ScannerPtr = std::unique_ptr<IScanner>;

} // namespace auracle::observation
