#include <assigned-numbers/assigned_numbers.hpp>

#include <cctype>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace auracle::assigned_numbers {

namespace {

constexpr std::string_view kSigBaseUuidSuffix = "-0000-1000-8000-00805f9b34fb";

[[nodiscard]] bool is_hex_digit(char c) noexcept {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] std::optional<std::uint16_t> parse_hex16(std::string_view value) noexcept {
    if (value.empty()) {
        return std::nullopt;
    }

    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        value.remove_prefix(2);
    }

    if (value.size() != 4) {
        return std::nullopt;
    }

    std::uint16_t parsed = 0;
    for (const char c : value) {
        if (!is_hex_digit(c)) {
            return std::nullopt;
        }

        parsed = static_cast<std::uint16_t>(parsed << 4);
        if (c >= '0' && c <= '9') {
            parsed = static_cast<std::uint16_t>(parsed + (c - '0'));
        } else if (c >= 'a' && c <= 'f') {
            parsed = static_cast<std::uint16_t>(parsed + (c - 'a' + 10));
        } else {
            parsed = static_cast<std::uint16_t>(parsed + (c - 'A' + 10));
        }
    }

    return parsed;
}

[[nodiscard]] bool iequals(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i]))
            != std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] std::optional<std::uint16_t> parse_service_uuid16(std::string_view uuid) noexcept {
    if (auto short_uuid = parse_hex16(uuid)) {
        return short_uuid;
    }

    if (uuid.size() != 36) {
        return std::nullopt;
    }

    if (!iequals(uuid.substr(8), kSigBaseUuidSuffix)) {
        return std::nullopt;
    }

    if (uuid.substr(0, 4) != "0000") {
        return std::nullopt;
    }

    return parse_hex16(uuid.substr(4, 4));
}

[[nodiscard]] std::string hex_bytes(std::span<const std::uint8_t> bytes) {
    std::string out;
    out.reserve(bytes.size() * 2U + 2U);
    out += "0x";
    for (const auto byte : bytes) {
        out += std::format("{:02X}", byte);
    }
    return out;
}

} // namespace

std::optional<std::string_view> service_name(std::string_view uuid) noexcept {
    const auto short_uuid = parse_service_uuid16(uuid);
    if (!short_uuid.has_value()) {
        return std::nullopt;
    }

    return service_name(*short_uuid);
}

std::optional<decoded_service_data> decode_service_data(
    std::string_view service_uuid,
    std::span<const std::uint8_t> payload) {
    const auto short_uuid = parse_service_uuid16(service_uuid);
    if (!short_uuid.has_value()) {
        return std::nullopt;
    }

    const auto label = service_name(*short_uuid).value_or(service_uuid);
    decoded_service_data decoded{
        .service_uuid = std::string(service_uuid),
        .service_label = std::string(label),
        .raw_value = std::vector<std::uint8_t>(payload.begin(), payload.end()),
        .fields = {},
    };

    switch (*short_uuid) {
    case 0x1852: {
        if (payload.size() >= 3U) {
            const auto broadcast_id = static_cast<std::uint32_t>(
                payload[0]
                | (payload[1] << 8U)
                | (payload[2] << 16U));
            decoded.fields.push_back({
                .field = "Broadcast ID",
                .type = "uint24",
                .value = std::format("0x{:06X}", broadcast_id),
            });
        } else {
            decoded.fields.push_back({
                .field = "Broadcast ID",
                .type = "invalid",
                .value = std::format("Expected 3 bytes, got {}", payload.size()),
            });
        }
        if (payload.size() > 3U) {
            decoded.fields.push_back({
                .field = "Trailing Data",
                .type = "bytes",
                .value = hex_bytes(payload.subspan(3U)),
            });
        }
        return decoded;
    }
    case 0x1856: {
        if (payload.empty()) {
            decoded.fields.push_back({
                .field = "Features",
                .type = "invalid",
                .value = "Missing Public Broadcast Announcement features byte",
            });
            return decoded;
        }

        const auto features = payload[0];
        decoded.fields.push_back({
            .field = "Features",
            .type = "bitfield8",
            .value = std::format("0x{:02X}", features),
        });

        if (payload.size() < 2U) {
            decoded.fields.push_back({
                .field = "Metadata Length",
                .type = "invalid",
                .value = "Missing metadata length byte",
            });
            return decoded;
        }

        const auto metadata_len = payload[1];
        decoded.fields.push_back({
            .field = "Metadata Length",
            .type = "uint8",
            .value = std::to_string(metadata_len),
        });

        const auto metadata_bytes = payload.subspan(2U);
        decoded.fields.push_back({
            .field = "Metadata",
            .type = "bytes",
            .value = hex_bytes(metadata_bytes),
        });
        if (metadata_len != metadata_bytes.size()) {
            decoded.fields.push_back({
                .field = "Metadata Status",
                .type = "warning",
                .value = std::format("Declared {} bytes, observed {} bytes", metadata_len, metadata_bytes.size()),
            });
        }
        return decoded;
    }
    default:
        return std::nullopt;
    }
}

} // namespace auracle::assigned_numbers
