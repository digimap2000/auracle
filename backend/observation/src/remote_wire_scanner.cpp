#include <observation/remote_wire_scanner.hpp>

#include <dts/ai.hpp>
#include <dts/log.hpp>
#include <dts/serial.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace auracle::observation {

namespace {

constexpr std::string_view log_tag = "remote-wire";
constexpr auto io_timeout = std::chrono::milliseconds{200};

class ITransport {
public:
    virtual ~ITransport() = default;
    [[nodiscard]] virtual std::expected<void, std::string> open() = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual std::expected<std::size_t, std::string>
    read_some(std::span<std::byte> buffer, std::stop_token stop) = 0;
    [[nodiscard]] virtual std::expected<void, std::string>
    write_all(std::span<const std::byte> buffer) = 0;
};

class SerialTransport final : public ITransport {
public:
    explicit SerialTransport(std::string path)
        : path_(std::move(path)) {}

    [[nodiscard]] std::expected<void, std::string> open() override {
        auto result = port_.open(path_, dts::serial::config{}, std::chrono::seconds{2});
        if (!result) {
            return std::unexpected(std::format(
                "failed to open serial port {}: {}",
                path_,
                result.error().message()));
        }
        return {};
    }

    void close() noexcept override {
        port_.close();
    }

    [[nodiscard]] std::expected<std::size_t, std::string>
    read_some(std::span<std::byte> buffer, std::stop_token /*stop*/) override {
        try {
            return port_.read(buffer, io_timeout);
        } catch (const std::system_error& e) {
            return std::unexpected(e.what());
        }
    }

    [[nodiscard]] std::expected<void, std::string>
    write_all(std::span<const std::byte> buffer) override {
        std::size_t written = 0;
        while (written < buffer.size()) {
            try {
                written += port_.write(buffer.subspan(written), io_timeout);
            } catch (const std::system_error& e) {
                return std::unexpected(e.what());
            }
        }
        return {};
    }

private:
    std::string path_;
    dts::serial::port port_;
};

class TcpTransport final : public ITransport {
public:
    TcpTransport(std::string host, std::uint16_t port)
        : host_(std::move(host))
        , port_(port) {}

    ~TcpTransport() override {
        close();
    }

    [[nodiscard]] std::expected<void, std::string> open() override {
        close();

        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        const auto port_str = std::to_string(port_);
        struct addrinfo* res{nullptr};
        if (::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res) != 0) {
            return std::unexpected(std::format("failed to resolve {}:{}", host_, port_));
        }

        int fd = -1;
        for (auto* rp = res; rp != nullptr; rp = rp->ai_next) {
            fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd < 0) {
                continue;
            }

            if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
                break;
            }

            ::close(fd);
            fd = -1;
        }

        ::freeaddrinfo(res);

        if (fd < 0) {
            return std::unexpected(std::format("failed to connect to {}:{}", host_, port_));
        }

        fd_ = fd;
        return {};
    }

    void close() noexcept override {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] std::expected<std::size_t, std::string>
    read_some(std::span<std::byte> buffer, std::stop_token stop) override {
        if (fd_ < 0) {
            return std::unexpected("tcp transport is not open");
        }

        struct pollfd pfd{.fd = fd_, .events = POLLIN, .revents = 0};
        int rc = ::poll(&pfd, 1, static_cast<int>(io_timeout.count()));
        if (rc == 0) {
            return std::size_t{0};
        }
        if (rc < 0) {
            return std::unexpected(std::strerror(errno));
        }
        if (stop.stop_requested()) {
            return std::size_t{0};
        }

        const auto n = ::recv(fd_, buffer.data(), buffer.size_bytes(), 0);
        if (n < 0) {
            return std::unexpected(std::strerror(errno));
        }
        if (n == 0) {
            return std::unexpected("tcp peer closed connection");
        }
        return static_cast<std::size_t>(n);
    }

    [[nodiscard]] std::expected<void, std::string>
    write_all(std::span<const std::byte> buffer) override {
        if (fd_ < 0) {
            return std::unexpected("tcp transport is not open");
        }

        std::size_t written = 0;
        while (written < buffer.size()) {
            const auto n = ::send(fd_, buffer.data() + written, buffer.size_bytes() - written, 0);
            if (n <= 0) {
                return std::unexpected(std::strerror(errno));
            }
            written += static_cast<std::size_t>(n);
        }
        return {};
    }

private:
    std::string host_;
    std::uint16_t port_{0};
    int fd_{-1};
};

struct ParsedAdvertisement {
    std::string name;
    std::optional<std::int32_t> tx_power;
    std::vector<std::string> service_uuids;
    std::vector<std::uint8_t> manufacturer_data;
    std::uint16_t company_id{0};
};

[[nodiscard]] std::string parse_text_field(std::span<const std::uint8_t> field) {
    std::size_t len = field.size();
    while (len > 0 && field[len - 1U] == 0) {
        len -= 1U;
    }

    return std::string(
        reinterpret_cast<const char*>(field.data()),
        reinterpret_cast<const char*>(field.data()) + len);
}

[[nodiscard]] std::string uuid16_to_string(std::uint16_t uuid) {
    return std::format("{:08x}-0000-1000-8000-00805f9b34fb", uuid);
}

[[nodiscard]] std::string uuid32_to_string(std::uint32_t uuid) {
    return std::format("{:08x}-0000-1000-8000-00805f9b34fb", uuid);
}

[[nodiscard]] std::string uuid128_to_string(const std::uint8_t* bytes) {
    std::array<std::uint8_t, 16> reversed{};
    for (std::size_t i = 0; i < reversed.size(); ++i) {
        reversed[i] = bytes[reversed.size() - 1 - i];
    }

    return std::format(
        "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        reversed[0], reversed[1], reversed[2], reversed[3],
        reversed[4], reversed[5],
        reversed[6], reversed[7],
        reversed[8], reversed[9],
        reversed[10], reversed[11], reversed[12], reversed[13], reversed[14], reversed[15]);
}

[[nodiscard]] std::optional<std::uint8_t> hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return static_cast<std::uint8_t>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return static_cast<std::uint8_t>(10 + (ch - 'a'));
    }
    if (ch >= 'A' && ch <= 'F') {
        return static_cast<std::uint8_t>(10 + (ch - 'A'));
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::uint8_t> hex_to_bytes(std::string_view hex) {
    std::vector<std::uint8_t> bytes;
    if ((hex.size() % 2U) != 0U) {
        return bytes;
    }

    bytes.reserve(hex.size() / 2U);
    for (std::size_t i = 0; i < hex.size(); i += 2U) {
        const auto hi = hex_nibble(hex[i]);
        const auto lo = hex_nibble(hex[i + 1U]);
        if (!hi.has_value() || !lo.has_value()) {
            bytes.clear();
            return bytes;
        }
        bytes.push_back(static_cast<std::uint8_t>((*hi << 4) | *lo));
    }
    return bytes;
}

void merge_payload(ParsedAdvertisement& parsed, std::span<const std::uint8_t> bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto field_len = bytes[offset];
        if (field_len == 0) {
            break;
        }

        if (offset + 1U + field_len > bytes.size()) {
            break;
        }

        const auto field_type = bytes[offset + 1U];
        const auto field = bytes.subspan(offset + 2U, field_len - 1U);

        switch (field_type) {
        case 0x08:
        case 0x09:
        case 0x30:
            if (parsed.name.empty() || field_type == 0x09) {
                parsed.name = parse_text_field(field);
            }
            break;
        case 0x0a:
            if (!field.empty()) {
                parsed.tx_power = static_cast<std::int8_t>(field[0]);
            }
            break;
        case 0x02:
        case 0x03:
            for (std::size_t i = 0; i + 1U < field.size(); i += 2U) {
                const auto uuid = static_cast<std::uint16_t>(field[i] | (field[i + 1U] << 8));
                parsed.service_uuids.push_back(uuid16_to_string(uuid));
            }
            break;
        case 0x04:
        case 0x05:
            for (std::size_t i = 0; i + 3U < field.size(); i += 4U) {
                const auto uuid = static_cast<std::uint32_t>(
                    field[i]
                    | (field[i + 1U] << 8)
                    | (field[i + 2U] << 16)
                    | (field[i + 3U] << 24));
                parsed.service_uuids.push_back(uuid32_to_string(uuid));
            }
            break;
        case 0x06:
        case 0x07:
            for (std::size_t i = 0; i + 15U < field.size(); i += 16U) {
                parsed.service_uuids.push_back(uuid128_to_string(field.data() + i));
            }
            break;
        case 0x16:
            if (field.size() >= 2U) {
                const auto uuid = static_cast<std::uint16_t>(field[0] | (field[1U] << 8));
                parsed.service_uuids.push_back(uuid16_to_string(uuid));
            }
            break;
        case 0x20:
            if (field.size() >= 4U) {
                const auto uuid = static_cast<std::uint32_t>(
                    field[0]
                    | (field[1U] << 8)
                    | (field[2U] << 16)
                    | (field[3U] << 24));
                parsed.service_uuids.push_back(uuid32_to_string(uuid));
            }
            break;
        case 0x21:
            if (field.size() >= 16U) {
                parsed.service_uuids.push_back(uuid128_to_string(field.data()));
            }
            break;
        case 0xff:
            if (field.size() >= 2U) {
                parsed.company_id = static_cast<std::uint16_t>(field[0] | (field[1U] << 8));
                parsed.manufacturer_data.assign(field.begin() + 2, field.end());
            }
            break;
        default:
            break;
        }

        offset += static_cast<std::size_t>(field_len) + 1U;
    }
}

[[nodiscard]] std::optional<BleAdvertisement> parse_line(std::string_view line) {
    dts::ai::json json;
    try {
        json = dts::ai::json::parse(line);
    } catch (...) {
        return std::nullopt;
    }

    if (!json.is_object()) {
        return std::nullopt;
    }

    const auto addr_it = json.find("addr");
    if (addr_it == json.end() || !addr_it->is_string()) {
        return std::nullopt;
    }

    std::optional<std::string> payload_hex;
    std::optional<std::string> scan_rsp_hex;
    std::optional<std::int32_t> top_level_tx_power;
    std::string address_type;
    std::int32_t rssi = -127;
    std::uint32_t sid = 255;
    std::uint32_t adv_type = 0;
    std::uint32_t adv_props = 0;
    std::uint32_t interval = 0;
    std::uint32_t primary_phy = 0;
    std::uint32_t secondary_phy = 0;

    const auto rssi_it = json.find("rssi");
    if (rssi_it != json.end() && rssi_it->is_number_integer()) {
        rssi = rssi_it->get<std::int32_t>();
    }

    const auto tx_power_it = json.find("tx_power");
    if (tx_power_it != json.end() && tx_power_it->is_number_integer()) {
        top_level_tx_power = tx_power_it->get<std::int32_t>();
    }

    const auto addr_type_it = json.find("addr_type");
    if (addr_type_it != json.end() && addr_type_it->is_string()) {
        address_type = addr_type_it->get<std::string>();
    }

    const auto sid_it = json.find("sid");
    if (sid_it != json.end() && sid_it->is_number_unsigned()) {
        sid = sid_it->get<std::uint32_t>();
    }

    const auto adv_type_it = json.find("adv_type");
    if (adv_type_it != json.end() && adv_type_it->is_number_unsigned()) {
        adv_type = adv_type_it->get<std::uint32_t>();
    }

    const auto adv_props_it = json.find("adv_props");
    if (adv_props_it != json.end() && adv_props_it->is_number_unsigned()) {
        adv_props = adv_props_it->get<std::uint32_t>();
    }

    const auto interval_it = json.find("interval");
    if (interval_it != json.end() && interval_it->is_number_unsigned()) {
        interval = interval_it->get<std::uint32_t>();
    }

    const auto primary_phy_it = json.find("primary_phy");
    if (primary_phy_it != json.end() && primary_phy_it->is_number_unsigned()) {
        primary_phy = primary_phy_it->get<std::uint32_t>();
    }

    const auto secondary_phy_it = json.find("secondary_phy");
    if (secondary_phy_it != json.end() && secondary_phy_it->is_number_unsigned()) {
        secondary_phy = secondary_phy_it->get<std::uint32_t>();
    }

    const auto type_it = json.find("type");
    if (type_it != json.end() && type_it->is_string() && *type_it == "adv") {
        const auto data_it = json.find("data");
        if (data_it != json.end() && data_it->is_string()) {
            payload_hex = data_it->get<std::string>();
        }

        const auto scan_rsp_it = json.find("scan_rsp");
        if (scan_rsp_it != json.end() && scan_rsp_it->is_string()) {
            scan_rsp_hex = scan_rsp_it->get<std::string>();
        }
    } else {
        const auto event_it = json.find("event");
        const auto data_hex_it = json.find("data_hex");
        if (event_it == json.end() || !event_it->is_string() || *event_it != "adv"
            ) {
            return std::nullopt;
        }

        if (data_hex_it != json.end() && data_hex_it->is_string()) {
            payload_hex = data_hex_it->get<std::string>();
        }
    }

    ParsedAdvertisement parsed;
    std::vector<std::uint8_t> data_bytes;
    if (payload_hex.has_value() && !payload_hex->empty()) {
        data_bytes = hex_to_bytes(*payload_hex);
        merge_payload(parsed, data_bytes);
    }

    std::vector<std::uint8_t> scan_rsp_bytes;
    if (scan_rsp_hex.has_value()) {
        scan_rsp_bytes = hex_to_bytes(*scan_rsp_hex);
        merge_payload(parsed, scan_rsp_bytes);
    }

    if (!parsed.tx_power.has_value() && top_level_tx_power.has_value()) {
        parsed.tx_power = *top_level_tx_power;
    }

    return BleAdvertisement{
        .device_id = addr_it->get<std::string>(),
        .name = parsed.name,
        .rssi = rssi,
        .tx_power = parsed.tx_power.value_or(127),
        .service_uuids = std::move(parsed.service_uuids),
        .manufacturer_data = std::move(parsed.manufacturer_data),
        .company_id = parsed.company_id,
        .address_type = std::move(address_type),
        .sid = sid,
        .adv_type = adv_type,
        .adv_props = adv_props,
        .interval = interval,
        .primary_phy = primary_phy,
        .secondary_phy = secondary_phy,
        .raw_data = std::move(data_bytes),
        .raw_scan_response = std::move(scan_rsp_bytes),
    };
}

struct LineStats {
    std::uint64_t total_lines{0};
    std::uint64_t adv_lines{0};
    std::uint64_t control_lines{0};
    std::uint64_t malformed_adv_lines{0};
    std::uint64_t unknown_lines{0};
    std::chrono::steady_clock::time_point last_stats_log{std::chrono::steady_clock::now()};
};

[[nodiscard]] bool is_adv_like_line(std::string_view line) {
    return line.find("\"event\":\"adv\"") != std::string_view::npos
        || line.find("\"type\":\"adv\"") != std::string_view::npos;
}

void maybe_log_control_line(std::string_view line, LineStats& stats) {
    dts::ai::json json;
    try {
        json = dts::ai::json::parse(line);
    } catch (...) {
        stats.unknown_lines += 1;
        if (stats.unknown_lines <= 3 || (stats.unknown_lines % 100) == 0) {
            dts::log::debug(log_tag, "non-json line from remote scanner: {:.120}", line);
        }
        return;
    }

    if (!json.is_object()) {
        stats.unknown_lines += 1;
        return;
    }

    const auto event_it = json.find("event");
    if (event_it != json.end() && event_it->is_string()) {
        stats.control_lines += 1;
        const auto event_name = event_it->get<std::string>();
        if (event_name == "ready") {
            dts::log::info(log_tag, "remote scanner signalled ready");
        } else if (event_name != "adv"
                   && (stats.control_lines <= 5 || (stats.control_lines % 100) == 0)) {
            dts::log::debug(log_tag, "remote scanner event={} line={:.160}", event_name, line);
        }
        return;
    }

    const auto id_it = json.find("id");
    const auto ok_it = json.find("ok");
    if (id_it != json.end() && id_it->is_number_integer()
        && ok_it != json.end() && ok_it->is_boolean()) {
        stats.control_lines += 1;
        const auto id = id_it->get<std::int32_t>();
        const auto ok = ok_it->get<bool>();
        if (ok) {
            dts::log::info(log_tag, "remote scanner response ok id={} line={:.160}", id, line);
        } else {
            std::string error = "unknown";
            const auto error_it = json.find("error");
            if (error_it != json.end() && error_it->is_string()) {
                error = error_it->get<std::string>();
            }
            dts::log::warn(log_tag, "remote scanner response failed id={} error={} line={:.160}", id, error, line);
        }
        return;
    }

    stats.unknown_lines += 1;
    if (stats.unknown_lines <= 3 || (stats.unknown_lines % 100) == 0) {
        dts::log::debug(log_tag, "unhandled remote scanner line: {:.160}", line);
    }
}

void maybe_log_stats(const LineStats& stats) {
    dts::log::debug(
        log_tag,
        "remote scanner stats lines={} adv={} control={} malformed_adv={} unknown={}",
        stats.total_lines,
        stats.adv_lines,
        stats.control_lines,
        stats.malformed_adv_lines,
        stats.unknown_lines);
}

[[nodiscard]] std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        bytes[i] = static_cast<std::byte>(text[i]);
    }
    return bytes;
}

[[nodiscard]] std::unique_ptr<ITransport>
make_transport(const RemoteWireScanner::Endpoint& endpoint) {
    return std::visit([](const auto& value) -> std::unique_ptr<ITransport> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, RemoteWireScanner::SerialEndpoint>) {
            return std::make_unique<SerialTransport>(value.path);
        } else {
            return std::make_unique<TcpTransport>(value.host, value.port);
        }
    }, endpoint);
}

} // namespace

RemoteWireScanner::RemoteWireScanner(const inventory::HardwareCandidate& candidate)
    : endpoint_([&candidate]() -> Endpoint {
        auto endpoint = endpoint_from_candidate(candidate);
        if (!endpoint) {
            throw std::runtime_error(endpoint.error());
        }
        return std::move(*endpoint);
    }())
    , protocol_(protocol_from_candidate(candidate)) {}

RemoteWireScanner::~RemoteWireScanner() {
    auto _ = stop();
}

void RemoteWireScanner::set_advertisement_handler(AdvertisementHandler handler) {
    std::scoped_lock lock(mutex_);
    handler_ = std::move(handler);
}

std::expected<void, std::string> RemoteWireScanner::start(bool /*allow_duplicates*/) {
    if (scanning_.exchange(true)) {
        return {};
    }

    worker_ = std::jthread([this](std::stop_token stop) {
        run(stop);
    });
    return {};
}

std::expected<void, std::string> RemoteWireScanner::stop() {
    if (!scanning_.exchange(false)) {
        return {};
    }

    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }

    return {};
}

bool RemoteWireScanner::scanning() const noexcept {
    return scanning_.load();
}

std::expected<RemoteWireScanner::Endpoint, std::string>
RemoteWireScanner::endpoint_from_candidate(const inventory::HardwareCandidate& candidate) {
    if (candidate.transport == inventory::Transport::Serial) {
        const auto* details = std::get_if<inventory::SerialDetails>(&candidate.details);
        if (details == nullptr || details->port.empty()) {
            return std::unexpected("serial candidate missing port details");
        }
        return SerialEndpoint{.path = details->port};
    }

    if (candidate.transport == inventory::Transport::Mdns) {
        const auto* details = std::get_if<inventory::MdnsDetails>(&candidate.details);
        if (details == nullptr || details->host.empty() || details->port == 0) {
            return std::unexpected("mdns candidate missing host/port details");
        }
        return TcpEndpoint{.host = details->host, .port = details->port};
    }

    return std::unexpected("candidate transport does not support remote wire scanning");
}

RemoteWireScanner::Protocol
RemoteWireScanner::protocol_from_candidate(const inventory::HardwareCandidate& candidate) {
    if (candidate.transport == inventory::Transport::Mdns) {
        return Protocol::EspConn;
    }

    return Protocol::WireTap;
}

void RemoteWireScanner::run(std::stop_token stop) {
    auto transport = make_transport(endpoint_);
    if (auto opened = transport->open(); !opened) {
        dts::log::warn(log_tag, "open failed: {}", opened.error());
        scanning_.store(false);
        return;
    }

    if (protocol_ == Protocol::WireTap) {
        const auto tap_enable = to_bytes("{\"type\":\"cmd\",\"action\":\"tap_enable\",\"tap\":\"adv\"}\n");
        if (auto sent = transport->write_all(tap_enable); !sent) {
            dts::log::warn(log_tag, "tap_enable failed: {}", sent.error());
            transport->close();
            scanning_.store(false);
            return;
        }
    } else {
        dts::log::info(log_tag, "remote scanner using passive esp-conn mode");
    }

    std::string line_buffer;
    std::array<std::byte, 512> buffer{};
    LineStats stats;
    auto start_time = std::chrono::steady_clock::now();

    while (!stop.stop_requested()) {
        auto bytes_read = transport->read_some(buffer, stop);
        if (!bytes_read) {
            dts::log::warn(log_tag, "read failed: {}", bytes_read.error());
            break;
        }

        if (*bytes_read == 0) {
            continue;
        }

        const auto* chars = reinterpret_cast<const char*>(buffer.data());
        line_buffer.append(chars, *bytes_read);

        std::size_t newline_pos = 0;
        while ((newline_pos = line_buffer.find('\n')) != std::string::npos) {
            auto line = line_buffer.substr(0, newline_pos);
            line_buffer.erase(0, newline_pos + 1U);

            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }

            if (line.empty()) {
                continue;
            }

            stats.total_lines += 1;
            auto advertisement = parse_line(line);
            if (!advertisement) {
                if (is_adv_like_line(line)) {
                    stats.malformed_adv_lines += 1;
                    if (stats.malformed_adv_lines <= 5 || (stats.malformed_adv_lines % 100) == 0) {
                        dts::log::warn(log_tag, "dropping malformed adv line: {:.160}", line);
                    }
                } else {
                    maybe_log_control_line(line, stats);
                }
                continue;
            }

            stats.adv_lines += 1;

            AdvertisementHandler handler;
            {
                std::scoped_lock lock(mutex_);
                handler = handler_;
            }

            if (handler) {
                handler(*advertisement);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - stats.last_stats_log >= std::chrono::seconds(5)) {
            maybe_log_stats(stats);
            if (stats.adv_lines == 0
                && stats.total_lines > 0
                && now - start_time >= std::chrono::seconds(5)) {
                dts::log::warn(
                    log_tag,
                    "remote scanner is receiving data but no advertisements are being promoted");
            }
            stats.last_stats_log = now;
        }
    }

    if (protocol_ == Protocol::WireTap) {
        const auto tap_disable = to_bytes("{\"type\":\"cmd\",\"action\":\"tap_disable\",\"tap\":\"adv\"}\n");
        auto _ = transport->write_all(tap_disable);
    }
    transport->close();
    scanning_.store(false);
}

} // namespace auracle::observation
