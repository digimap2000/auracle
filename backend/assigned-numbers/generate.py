#!/usr/bin/env python3
"""Generate C++ lookup tables from Bluetooth SIG assigned numbers YAML.

Reads the official YAML files from the bluetooth-SIG/public repository and
produces a single C++ source file with sorted constexpr arrays and binary-
search lookup functions.

Usage:
    python3 generate.py <input_dir> <output_dir>

Where <input_dir> is the path to the assigned_numbers/ directory inside the
Bluetooth SIG repo and <output_dir> is where assigned_numbers_data.cpp will
be written.

Dependencies: Python 3.10+, PyYAML.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml


# ---------------------------------------------------------------------------
# YAML parsing
# ---------------------------------------------------------------------------


def _clean_name(name: str) -> str:
    """Strip LaTeX artifacts and normalise whitespace."""
    name = re.sub(r"\\textsubscript\{([^}]*)\}", r"\1", name)
    return name.strip()


def _c_escape(s: str) -> str:
    """Escape a string for embedding in a C++ string literal."""
    return s.replace("\\", "\\\\").replace('"', '\\"')


def _parse_pairs(
    filepath: Path, key_field: str = "value", root_key: str | None = None,
) -> list[tuple[int, str]]:
    """Parse a YAML file with key/name pairs into a sorted (int, str) list.

    Each entry is expected to have *key_field* (a hex string like '0x1800')
    and a 'name' field.  The list under *root_key* is used; if None the
    first top-level list-valued key is used automatically.
    """
    with filepath.open(encoding="utf-8") as fh:
        data = yaml.safe_load(fh)

    if root_key is not None:
        items = data[root_key]
    else:
        # Pick the first list-valued key at the top level.
        items = next(v for v in data.values() if isinstance(v, list))

    entries: list[tuple[int, str]] = []
    for item in items:
        raw_key = item.get(key_field)
        raw_name = item.get("name")
        if raw_key is None or raw_name is None:
            continue
        key = int(str(raw_key), 16) if isinstance(raw_key, str) else int(raw_key)
        entries.append((key, _clean_name(str(raw_name))))

    entries.sort(key=lambda e: e[0])
    return entries


def _titleize_words(stem: str) -> str:
    special = {
        "ccid": "CCID",
        "uri": "URI",
    }
    words = []
    for part in stem.split("_"):
        words.append(special.get(part, part.capitalize()))
    return " ".join(words)


def _parse_metadata_ltv_types(directory: Path) -> list[tuple[int, str]]:
    entries: list[tuple[int, str]] = []

    for filepath in sorted(directory.glob("*.yaml")):
        with filepath.open(encoding="utf-8") as fh:
            data = yaml.safe_load(fh)

        items = data.get("supported_ltv_structure", [])
        type_value = None
        for item in items:
            if item.get("parameter") == "Type":
                raw_value = item.get("value")
                if isinstance(raw_value, str) and raw_value.startswith("0x"):
                    type_value = int(raw_value, 16)
                elif isinstance(raw_value, int):
                    type_value = raw_value
                break

        if type_value is None:
            continue

        entries.append((type_value, _titleize_words(filepath.stem)))

    entries.sort(key=lambda e: e[0])
    return entries


def _service_data_symbol(stem: str) -> str:
    return "".join(part.capitalize() for part in stem.split("_"))


def _parse_int(raw: object, filepath: Path, label: str) -> int:
    if isinstance(raw, str):
        try:
            return int(raw, 16 if raw.lower().startswith("0x") else 10)
        except ValueError as exc:
            raise ValueError(f"{filepath}: invalid {label} {raw!r}") from exc
    if isinstance(raw, int):
        return raw
    raise ValueError(f"{filepath}: invalid {label} {raw!r}")


def _parse_service_data_formats(directory: Path) -> list[dict]:
    kind_values = {"uint8", "uint24", "bytes_remainder"}
    format_values = {"decimal", "hex"}
    enum_match_values = {"exact", "bitfield_all"}
    validation_values = {"bytes_field_matches_u8_length"}

    formats: list[dict] = []
    for filepath in sorted(directory.glob("*.yaml")):
        with filepath.open(encoding="utf-8") as fh:
            data = yaml.safe_load(fh)

        raw_uuid = data.get("uuid")
        if raw_uuid is None:
            raise ValueError(f"{filepath}: missing uuid")
        uuid = _parse_int(raw_uuid, filepath, "uuid")

        name = str(data.get("name", "")).strip()
        description = str(data.get("description", "")).strip()
        if not name:
            raise ValueError(f"{filepath}: missing name")
        if not description:
            raise ValueError(f"{filepath}: missing description")

        fields_raw = data.get("fields", [])
        fields: list[dict] = []
        field_indices: dict[str, int] = {}
        for index, field in enumerate(fields_raw):
            field_name = str(field["field"])
            if field_name in field_indices:
                raise ValueError(f"{filepath}: duplicate field name {field_name!r}")

            kind = str(field["kind"])
            if kind not in kind_values:
                raise ValueError(f"{filepath}: unsupported field kind {kind!r}")

            numeric_format = str(field.get("format", "decimal"))
            if numeric_format not in format_values:
                raise ValueError(f"{filepath}: unsupported numeric format {numeric_format!r}")

            enum_match = str(field.get("enum_match", "exact"))
            if enum_match not in enum_match_values:
                raise ValueError(f"{filepath}: unsupported enum_match {enum_match!r}")

            enumerations: list[dict] = []
            for enum_entry in field.get("enumerations", []):
                short_name = str(enum_entry.get("short_name", "")).strip()
                enum_description = str(enum_entry.get("description", "")).strip()
                if not short_name:
                    raise ValueError(f"{filepath}: enumeration on {field_name!r} is missing short_name")
                if not enum_description:
                    raise ValueError(f"{filepath}: enumeration on {field_name!r} is missing description")
                implications_raw = enum_entry.get("implications", [])
                if implications_raw is None:
                    implications_raw = []
                if not isinstance(implications_raw, list):
                    raise ValueError(
                        f"{filepath}: implications on {field_name!r} must be a list of strings")
                implications = []
                for implication in implications_raw:
                    implication_text = str(implication).strip()
                    if not implication_text:
                        raise ValueError(
                            f"{filepath}: empty implication on {field_name!r} is not allowed")
                    implications.append(implication_text)
                enumerations.append({
                    "value": _parse_int(enum_entry.get("value"), filepath, f"enumeration value for {field_name}"),
                    "short_name": short_name,
                    "description": enum_description,
                    "implications": implications,
                })

            field_indices[field_name] = index
            fields.append({
                "field": field_name,
                "type": str(field["type"]),
                "kind": kind,
                "format": numeric_format,
                "enum_match": enum_match,
                "enumerations": enumerations,
                "omit_if_empty": bool(field.get("omit_if_empty", False)),
            })

        validations_raw = data.get("validations", [])
        validations: list[dict] = []
        for validation in validations_raw:
            validation_kind = str(validation["kind"])
            if validation_kind not in validation_values:
                raise ValueError(f"{filepath}: unsupported validation kind {validation_kind!r}")

            value_field = str(validation["value_field"])
            bytes_field = str(validation["bytes_field"])
            if value_field not in field_indices:
                raise ValueError(f"{filepath}: unknown validation value_field {value_field!r}")
            if bytes_field not in field_indices:
                raise ValueError(f"{filepath}: unknown validation bytes_field {bytes_field!r}")

            validations.append({
                "kind": validation_kind,
                "value_field_index": field_indices[value_field],
                "bytes_field_index": field_indices[bytes_field],
            })

        formats.append({
            "uuid": uuid,
            "name": name,
            "description": description,
            "symbol": _service_data_symbol(filepath.stem),
            "fields": fields,
            "validations": validations,
        })

    return formats


# ---------------------------------------------------------------------------
# C++ code generation
# ---------------------------------------------------------------------------

_HEADER = """\
// AUTO-GENERATED by generate.py — do not edit.
// Source: https://bitbucket.org/bluetooth-SIG/public

#include <assigned-numbers/assigned_numbers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace auracle::assigned_numbers {
namespace {

struct entry16 {
    std::uint16_t key;
    std::string_view name;
};

struct entry8 {
    std::uint8_t key;
    std::string_view name;
};

template <typename Entry, std::size_t N>
[[nodiscard]] std::optional<std::string_view>
lookup(const std::array<Entry, N>& table,
       decltype(Entry::key) key) noexcept {
    auto it = std::lower_bound(
        table.begin(), table.end(), key,
        [](const Entry& e, auto k) { return e.key < k; });
    if (it != table.end() && it->key == key) return it->name;
    return std::nullopt;
}

"""

_SERVICE_DATA_FORMATS_HEADER = """\
// AUTO-GENERATED by generate.py — do not edit.

#include "service_data_formats.hpp"

#include <array>

namespace auracle::assigned_numbers::detail {
namespace {

"""


def _emit_array(
    entries: list[tuple[int, str]],
    name: str,
    entry_type: str,
    hex_width: int,
) -> str:
    """Emit a constexpr std::array definition."""
    lines = [
        f"constexpr std::array<{entry_type}, {len(entries)}> {name} {{{{"
    ]
    for key, value in entries:
        escaped = _c_escape(value)
        lines.append(f'    {{0x{key:0{hex_width}X}, "{escaped}"}},')
    lines.append("}};\n")
    return "\n".join(lines)


def _emit_function(
    func_name: str,
    param_type: str,
    array_name: str,
) -> str:
    """Emit a lookup function definition."""
    return (
        f"std::optional<std::string_view> {func_name}"
        f"({param_type} v) noexcept {{\n"
        f"    return lookup({array_name}, v);\n"
        f"}}\n"
    )


def _emit_service_data_formats(formats: list[dict]) -> str:
    kind_map = {
        "uint8": "service_data_field_kind::uint8",
        "uint24": "service_data_field_kind::uint24",
        "bytes_remainder": "service_data_field_kind::bytes_remainder",
    }
    format_map = {
        "decimal": "service_data_numeric_format::decimal",
        "hex": "service_data_numeric_format::hex",
    }
    enum_match_map = {
        "exact": "service_data_enum_match_kind::exact",
        "bitfield_all": "service_data_enum_match_kind::bitfield_all",
    }
    validation_map = {
        "bytes_field_matches_u8_length":
            "service_data_validation_kind::bytes_field_matches_u8_length",
    }

    parts = [_SERVICE_DATA_FORMATS_HEADER]

    for fmt in formats:
        fields = fmt["fields"]
        validations = fmt["validations"]
        fields_name = f'k{fmt["symbol"]}Fields'
        validations_name = f'k{fmt["symbol"]}Validations'

        for index, field in enumerate(fields):
            enum_entries = field["enumerations"]
            if not enum_entries:
                continue

            for entry_index, entry in enumerate(enum_entries):
                implications = entry["implications"]
                if not implications:
                    continue

                implications_name = (
                    f'k{fmt["symbol"]}Field{index}EnumEntry{entry_index}Implications'
                )
                parts.append(
                    f"constexpr std::array<std::string_view, {len(implications)}> "
                    f"{implications_name}{{{{"
                )
                for implication in implications:
                    parts.append(f'    "{_c_escape(implication)}",')
                parts.append("}};\n")

            enum_entries_name = f'k{fmt["symbol"]}Field{index}EnumEntries'
            parts.append(
                f"constexpr std::array<service_data_enum_entry_definition, {len(enum_entries)}> "
                f"{enum_entries_name}{{{{"
            )
            for entry_index, entry in enumerate(enum_entries):
                parts.append("    service_data_enum_entry_definition{")
                parts.append(f'        .value = 0x{entry["value"]:X},')
                parts.append(f'        .short_name = "{_c_escape(entry["short_name"])}",')
                parts.append(f'        .description = "{_c_escape(entry["description"])}",')
                if entry["implications"]:
                    parts.append(
                        f'        .implications = '
                        f'k{fmt["symbol"]}Field{index}EnumEntry{entry_index}Implications,'
                    )
                parts.append("    },")
            parts.append("}};\n")

        parts.append(
            f"constexpr std::array<service_data_field_definition, {len(fields)}> "
            f"{fields_name}{{{{"
        )
        for index, field in enumerate(fields):
            parts.append("    service_data_field_definition{")
            parts.append(f'        .field = "{_c_escape(field["field"])}",')
            parts.append(f'        .type = "{_c_escape(field["type"])}",')
            parts.append(f'        .kind = {kind_map[field["kind"]]},')
            parts.append(f'        .numeric_format = {format_map[field["format"]]},')
            if field["enumerations"]:
                parts.append(f'        .enum_match = {enum_match_map[field["enum_match"]]},')
                parts.append(
                    f'        .enum_entries = k{fmt["symbol"]}Field{index}EnumEntries,'
                )
            parts.append(
                f'        .omit_if_empty = {"true" if field["omit_if_empty"] else "false"},'
            )
            parts.append("    },")
        parts.append("}};\n")

        parts.append(
            f"constexpr std::array<service_data_validation_definition, {len(validations)}> "
            f"{validations_name}{{{{"
        )
        for validation in validations:
            parts.append("    service_data_validation_definition{")
            parts.append(f'        .kind = {validation_map[validation["kind"]]},')
            parts.append(f'        .value_field_index = {validation["value_field_index"]},')
            parts.append(f'        .bytes_field_index = {validation["bytes_field_index"]},')
            parts.append("    },")
        parts.append("}};\n")

    parts.append(
        f"constexpr std::array<service_data_format_definition, {len(formats)}> "
        "kServiceDataFormats{{"
    )
    for fmt in formats:
        parts.append("    service_data_format_definition{")
        parts.append(f'        .uuid = 0x{fmt["uuid"]:04X},')
        parts.append(f'        .name = "{_c_escape(fmt["name"])}",')
        parts.append(f'        .description = "{_c_escape(fmt["description"])}",')
        parts.append(f'        .fields = k{fmt["symbol"]}Fields,')
        parts.append(f'        .validations = k{fmt["symbol"]}Validations,')
        parts.append("    },")
    parts.append("}};\n")

    parts.append("} // anonymous namespace\n")
    parts.append(
        "const service_data_format_definition* find_service_data_format"
        "(std::uint16_t uuid) noexcept {\n"
        "    for (const auto& format : kServiceDataFormats) {\n"
        "        if (format.uuid == uuid) {\n"
        "            return &format;\n"
        "        }\n"
        "    }\n"
        "    return nullptr;\n"
        "}\n"
    )
    parts.append("} // namespace auracle::assigned_numbers::detail\n")
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_dir> <output_dir>", file=sys.stderr)
        sys.exit(1)

    input_dir = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    # Parse YAML files
    companies = _parse_pairs(
        input_dir / "company_identifiers" / "company_identifiers.yaml",
        root_key="company_identifiers",
    )
    services = _parse_pairs(
        input_dir / "uuids" / "service_uuids.yaml",
        key_field="uuid",
        root_key="uuids",
    )
    characteristics = _parse_pairs(
        input_dir / "uuids" / "characteristic_uuids.yaml",
        key_field="uuid",
        root_key="uuids",
    )
    ad_types = _parse_pairs(
        input_dir / "core" / "ad_types.yaml",
        root_key="ad_types",
    )
    metadata_types = _parse_metadata_ltv_types(
        input_dir / "profiles_and_services" / "generic_audio" / "metadata_ltv"
    )
    service_data_formats = _parse_service_data_formats(
        Path(__file__).resolve().parent / "service-data-formats"
    )

    # Sanity checks
    if len(companies) < 3000:
        print(
            f"ERROR: Only {len(companies)} company identifiers parsed "
            f"(expected >3000). YAML format may have changed.",
            file=sys.stderr,
        )
        sys.exit(1)
    if len(services) < 50:
        print(
            f"ERROR: Only {len(services)} service UUIDs parsed "
            f"(expected >50). YAML format may have changed.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Generate C++
    parts = [_HEADER]

    parts.append(_emit_array(companies, "kCompanyIds", "entry16", 4))
    parts.append(_emit_array(services, "kServiceUuids", "entry16", 4))
    parts.append(
        _emit_array(characteristics, "kCharacteristicUuids", "entry16", 4)
    )
    parts.append(_emit_array(ad_types, "kAdTypes", "entry8", 2))
    parts.append(_emit_array(metadata_types, "kMetadataTypes", "entry8", 2))

    parts.append("} // anonymous namespace\n")

    parts.append(
        _emit_function("company_name", "std::uint16_t", "kCompanyIds")
    )
    parts.append(
        _emit_function("service_name", "std::uint16_t", "kServiceUuids")
    )
    parts.append(
        _emit_function(
            "characteristic_name", "std::uint16_t", "kCharacteristicUuids"
        )
    )
    parts.append(
        _emit_function("ad_type_name", "std::uint8_t", "kAdTypes")
    )
    parts.append(
        _emit_function("metadata_type_name", "std::uint8_t", "kMetadataTypes")
    )

    parts.append("} // namespace auracle::assigned_numbers\n")

    # Write output
    output_dir.mkdir(parents=True, exist_ok=True)
    output_file = output_dir / "assigned_numbers_data.cpp"
    output_file.write_text("\n".join(parts), encoding="utf-8")
    service_data_output = output_dir / "service_data_formats_generated.cpp"
    service_data_output.write_text(
        _emit_service_data_formats(service_data_formats), encoding="utf-8"
    )

    print(
        f"Generated {output_file}: "
        f"{len(companies)} companies, "
        f"{len(services)} services, "
        f"{len(characteristics)} characteristics, "
        f"{len(ad_types)} AD types, "
        f"{len(service_data_formats)} service packet formats"
    )


if __name__ == "__main__":
    main()
