#pragma once

#include "scanner.hpp"

#include <inventory/candidate.hpp>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <variant>

namespace auracle::observation {

class RemoteWireScanner final : public IScanner {
public:
    enum class Protocol {
        WireTap,
        EspConn,
    };

    struct SerialEndpoint {
        std::string path;
    };

    struct TcpEndpoint {
        std::string host;
        std::uint16_t port{0};
    };

    using Endpoint = std::variant<SerialEndpoint, TcpEndpoint>;

    explicit RemoteWireScanner(const inventory::HardwareCandidate& candidate);
    ~RemoteWireScanner() override;

    void set_advertisement_handler(AdvertisementHandler handler) override;
    [[nodiscard]] std::expected<void, std::string> start(bool allow_duplicates) override;
    [[nodiscard]] std::expected<void, std::string> stop() override;
    [[nodiscard]] bool scanning() const noexcept override;

private:
    [[nodiscard]] static std::expected<Endpoint, std::string>
    endpoint_from_candidate(const inventory::HardwareCandidate& candidate);
    [[nodiscard]] static Protocol
    protocol_from_candidate(const inventory::HardwareCandidate& candidate);

    void run(std::stop_token stop);

    Endpoint endpoint_;
    Protocol protocol_;
    mutable std::mutex mutex_;
    AdvertisementHandler handler_;
    std::jthread worker_;
    std::atomic<bool> scanning_{false};
};

} // namespace auracle::observation
