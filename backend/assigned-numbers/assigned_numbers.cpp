#include <assigned-numbers/assigned_numbers.hpp>

#include "service_data_formats.hpp"

#include <cctype>
#include <cstddef>
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

[[nodiscard]] std::string_view service_data_status_text(std::uint16_t code) noexcept {
    switch (code) {
    case 200:
        return "OK";
    case 400:
        return "Invalid Service UUID";
    case 404:
        return "Unknown Service Packet Format";
    case 422:
        return "Malformed Service Packet";
    case 500:
        return "Invalid Service Packet Definition";
    default:
        return "Unknown Status";
    }
}

[[nodiscard]] decoded_service_data make_decoded_service_data(
    std::string_view service_uuid,
    std::string_view service_label,
    std::span<const std::uint8_t> payload) {
    return decoded_service_data{
        .service_uuid = std::string(service_uuid),
        .service_label = std::string(service_label),
        .raw_value = std::vector<std::uint8_t>(payload.begin(), payload.end()),
        .fields = {},
        .status_code = 200,
        .status_message = std::string(service_data_status_text(200)),
    };
}

[[nodiscard]] service_data_format_metadata make_service_data_format_metadata(
    std::string_view service_uuid,
    std::string_view service_label) {
    return service_data_format_metadata{
        .service_uuid = std::string(service_uuid),
        .service_label = std::string(service_label),
        .service_description = {},
        .fields = {},
        .status_code = 200,
        .status_message = std::string(service_data_status_text(200)),
    };
}

void set_status(decoded_service_data* decoded, std::uint16_t code, std::string message) {
    decoded->status_code = code;
    decoded->status_message = message;
}

void set_status(service_data_format_metadata* metadata, std::uint16_t code, std::string message) {
    metadata->status_code = code;
    metadata->status_message = message;
}

[[nodiscard]] std::string format_unsigned(
    std::uint32_t value,
    std::size_t width_bytes,
    detail::service_data_numeric_format format) {
    switch (format) {
    case detail::service_data_numeric_format::decimal:
        return std::to_string(value);
    case detail::service_data_numeric_format::hex:
        return std::format("0x{:0{}X}", value, static_cast<int>(width_bytes * 2U));
    }

    return std::to_string(value);
}

struct parsed_field_state {
    bool has_unsigned_value = false;
    std::uint32_t unsigned_value = 0;
    std::size_t byte_size = 0;
};

[[nodiscard]] decoded_service_data parse_service_data_payload(
    std::string_view service_uuid,
    std::string_view service_label,
    std::span<const std::uint8_t> payload,
    const detail::service_data_format_definition& format) {
    decoded_service_data decoded = make_decoded_service_data(service_uuid, service_label, payload);
    std::vector<parsed_field_state> parsed_fields;
    parsed_fields.reserve(format.fields.size());

    std::size_t offset = 0;
    for (const auto& field : format.fields) {
        switch (field.kind) {
        case detail::service_data_field_kind::uint8: {
            constexpr std::size_t kFieldSize = 1U;
            if (offset + kFieldSize > payload.size()) {
                set_status(
                    &decoded,
                    422,
                    std::format(
                        "{}: expected {} byte for {}",
                        service_data_status_text(422),
                        kFieldSize,
                        field.field));
                return decoded;
            }

            const auto value = payload[offset];
            offset += kFieldSize;
            decoded.fields.push_back({
                .field = std::string(field.field),
                .type = std::string(field.type),
                .value = format_unsigned(value, kFieldSize, field.numeric_format),
            });
            parsed_fields.push_back({
                .has_unsigned_value = true,
                .unsigned_value = value,
                .byte_size = kFieldSize,
            });
            break;
        }
        case detail::service_data_field_kind::uint24: {
            constexpr std::size_t kFieldSize = 3U;
            if (offset + kFieldSize > payload.size()) {
                set_status(
                    &decoded,
                    422,
                    std::format(
                        "{}: expected {} bytes for {}",
                        service_data_status_text(422),
                        kFieldSize,
                        field.field));
                return decoded;
            }

            const auto value = static_cast<std::uint32_t>(
                payload[offset]
                | (payload[offset + 1U] << 8U)
                | (payload[offset + 2U] << 16U));
            offset += kFieldSize;
            decoded.fields.push_back({
                .field = std::string(field.field),
                .type = std::string(field.type),
                .value = format_unsigned(value, kFieldSize, field.numeric_format),
            });
            parsed_fields.push_back({
                .has_unsigned_value = true,
                .unsigned_value = value,
                .byte_size = kFieldSize,
            });
            break;
        }
        case detail::service_data_field_kind::bytes_remainder: {
            const auto bytes = payload.subspan(offset);
            offset = payload.size();
            if (!field.omit_if_empty || !bytes.empty()) {
                decoded.fields.push_back({
                    .field = std::string(field.field),
                    .type = std::string(field.type),
                    .value = hex_bytes(bytes),
                });
            }
            parsed_fields.push_back({
                .byte_size = bytes.size(),
            });
            break;
        }
        }
    }

    for (const auto& validation : format.validations) {
        switch (validation.kind) {
        case detail::service_data_validation_kind::bytes_field_matches_u8_length: {
            if (validation.value_field_index >= parsed_fields.size()
                || validation.bytes_field_index >= parsed_fields.size()) {
                set_status(
                    &decoded,
                    500,
                    std::format(
                        "{}: validation indices out of range for service 0x{:04X}",
                        service_data_status_text(500),
                        format.uuid));
                return decoded;
            }

            const auto& value_field = parsed_fields[validation.value_field_index];
            const auto& bytes_field = parsed_fields[validation.bytes_field_index];
            if (!value_field.has_unsigned_value) {
                set_status(
                    &decoded,
                    500,
                    std::format(
                        "{}: length field is not numeric for service 0x{:04X}",
                        service_data_status_text(500),
                        format.uuid));
                return decoded;
            }

            if (bytes_field.byte_size != value_field.unsigned_value) {
                set_status(
                    &decoded,
                    422,
                    std::format(
                        "{}: declared {} bytes, observed {} bytes",
                        service_data_status_text(422),
                        value_field.unsigned_value,
                        bytes_field.byte_size));
                return decoded;
            }
            break;
        }
        }
    }

    return decoded;
}

} // namespace

std::optional<std::string_view> service_name(std::string_view uuid) noexcept {
    const auto short_uuid = parse_service_uuid16(uuid);
    if (!short_uuid.has_value()) {
        return std::nullopt;
    }

    return service_name(*short_uuid);
}

service_data_format_metadata describe_service_data_format(std::string_view service_uuid) {
    const auto fallback_label = service_name(service_uuid).value_or(service_uuid);
    service_data_format_metadata metadata = make_service_data_format_metadata(
        service_uuid, fallback_label);

    const auto short_uuid = parse_service_uuid16(service_uuid);
    if (!short_uuid.has_value()) {
        set_status(
            &metadata,
            400,
            std::string(service_data_status_text(400)));
        return metadata;
    }

    const auto* format = detail::find_service_data_format(*short_uuid);
    if (format == nullptr) {
        set_status(
            &metadata,
            404,
            std::string(service_data_status_text(404)));
        return metadata;
    }

    metadata.service_label = std::string(format->name.empty()
        ? service_name(*short_uuid).value_or(service_uuid)
        : format->name);
    metadata.service_description = std::string(format->description);
    metadata.fields.reserve(format->fields.size());

    for (const auto& field : format->fields) {
        service_data_field_metadata field_metadata{
            .field = std::string(field.field),
            .type = std::string(field.type),
            .enum_match = field.enum_entries.empty()
                ? ""
                : std::string(field.enum_match == detail::service_data_enum_match_kind::bitfield_all
                    ? "bitfield_all"
                    : "exact"),
            .enum_entries = {},
        };
        field_metadata.enum_entries.reserve(field.enum_entries.size());
        for (const auto& entry : field.enum_entries) {
            field_metadata.enum_entries.push_back({
                .value = entry.value,
                .short_name = std::string(entry.short_name),
                .description = std::string(entry.description),
                .implications = [&entry] {
                    std::vector<std::string> implications;
                    implications.reserve(entry.implications.size());
                    for (const auto implication : entry.implications) {
                        implications.push_back(std::string(implication));
                    }
                    return implications;
                }(),
            });
        }
        metadata.fields.push_back(std::move(field_metadata));
    }

    return metadata;
}

decoded_service_data decode_service_data(
    std::string_view service_uuid,
    std::span<const std::uint8_t> payload) {
    const auto fallback_label = service_name(service_uuid).value_or(service_uuid);
    decoded_service_data decoded = make_decoded_service_data(service_uuid, fallback_label, payload);

    const auto short_uuid = parse_service_uuid16(service_uuid);
    if (!short_uuid.has_value()) {
        set_status(
            &decoded,
            400,
            std::string(service_data_status_text(400)));
        return decoded;
    }

    const auto* format = detail::find_service_data_format(*short_uuid);
    if (format == nullptr) {
        set_status(
            &decoded,
            404,
            std::string(service_data_status_text(404)));
        return decoded;
    }

    const auto label = format->name.empty()
        ? service_name(*short_uuid).value_or(service_uuid)
        : format->name;
    return parse_service_data_payload(service_uuid, label, payload, *format);
}

} // namespace auracle::assigned_numbers
