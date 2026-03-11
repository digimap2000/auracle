#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace auracle::assigned_numbers::detail {

enum class service_data_field_kind {
    uint8,
    uint24,
    bytes_remainder,
};

enum class service_data_numeric_format {
    decimal,
    hex,
};

enum class service_data_enum_match_kind {
    exact,
    bitfield_all,
};

struct service_data_enum_entry_definition {
    std::uint32_t value;
    std::string_view short_name;
    std::string_view description;
    std::span<const std::string_view> implications;
};

struct service_data_field_definition {
    std::string_view field;
    std::string_view type;
    service_data_field_kind kind;
    service_data_numeric_format numeric_format = service_data_numeric_format::decimal;
    service_data_enum_match_kind enum_match = service_data_enum_match_kind::exact;
    std::span<const service_data_enum_entry_definition> enum_entries;
    bool omit_if_empty = false;
};

enum class service_data_validation_kind {
    bytes_field_matches_u8_length,
};

struct service_data_validation_definition {
    service_data_validation_kind kind;
    std::size_t value_field_index;
    std::size_t bytes_field_index;
};

struct service_data_format_definition {
    std::uint16_t uuid;
    std::string_view name;
    std::string_view description;
    std::span<const service_data_field_definition> fields;
    std::span<const service_data_validation_definition> validations;
};

[[nodiscard]] const service_data_format_definition* find_service_data_format(std::uint16_t uuid) noexcept;

} // namespace auracle::assigned_numbers::detail
