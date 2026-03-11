# Service Data Format Authoring

Write one YAML file per 16-bit service UUID in this folder.

## What These Files Are

These YAML files declare the byte layout of BLE `Service Data - 16-bit UUID` payloads.

They are build inputs only:

1. add or edit YAML here
2. run the normal backend build
3. `generate.py` validates the YAML and emits C++ tables
4. the daemon and CLI use the generated tables through one generic parser

Do not add hand-written per-UUID parsing code in `assigned_numbers.cpp`.

The data in these files serves two different runtime paths:

- packet decode: parses bytes into field values only
- format description: returns the static metadata such as service descriptions and enumeration tables

That split is intentional so repeated packet decode does not pay to resend descriptive metadata every time.

## File Naming

Use:

`<uuid>_<short_name>.yaml`

Examples:

- `1852_broadcast_audio_announcement.yaml`
- `1856_public_broadcast_announcement.yaml`

Use lowercase hex for the filename stem and snake_case for the descriptive suffix.

## Minimal Example

```yaml
uuid: 0x1856
name: Public Broadcast Announcement
description: Carries the public broadcast feature flags and optional metadata.
fields:
  - field: Features
    type: bitfield8
    kind: uint8
    format: hex
    enum_match: bitfield_all
    enumerations:
      - value: 0x01
        short_name: encrypted
        description: Broadcast audio is encrypted.
  - field: Metadata Length
    type: uint8
    kind: uint8
  - field: Metadata
    type: bytes
    kind: bytes_remainder
validations:
  - kind: bytes_field_matches_u8_length
    value_field: Metadata Length
    bytes_field: Metadata
```

## Top-Level Keys

- `uuid`: Required. The 16-bit service UUID as a number or hex literal.
- `name`: Required. Canonical human-readable name for this service-data format.
- `description`: Required. Human-readable description for the service and its packet purpose.
- `fields`: Required. Ordered list of parse steps. Order matters.
- `validations`: Optional. Cross-field checks run after parsing.

## Field Properties

- `field`: Required. Human-readable label returned in decoded output.
- `type`: Required. Semantic output type label returned in decoded output.
- `kind`: Required. Binary parse primitive.
- `format`: Optional. Numeric display format. Defaults to `decimal`.
- `omit_if_empty`: Optional. Only meaningful for `bytes_remainder`. If `true`, an empty field is omitted from the decoded output.
- `enum_match`: Optional. How `enumerations` are matched. Defaults to `exact`.
- `enumerations`: Optional. Named values or bit masks attached to a numeric field.

## Supported Field Kinds

- `uint8`
  Reads 1 byte as an unsigned integer.

- `uint24`
  Reads 3 bytes as a little-endian unsigned integer.

- `bytes_remainder`
  Consumes all remaining bytes and returns them as a hex string.

## Supported Numeric Formats

- `decimal`
- `hex`

## Supported Enumeration Matching

- `exact`
  Emits entries whose declared `value` exactly equals the parsed numeric field value.

- `bitfield_all`
  Emits entries whose declared `value` bit mask is fully set in the parsed numeric field value.

Enumeration entries use:

```yaml
enumerations:
  - value: 0x04
    short_name: high_quality
    description: The public broadcast offers a high quality audio presentation.
```

- `value`: Required numeric value or bit mask.
- `short_name`: Required compact identifier.
- `description`: Required human-readable meaning.

## Supported Validations

- `bytes_field_matches_u8_length`
  Use this when one `uint8` field declares the byte count of a later `bytes_remainder` field.

Example:

```yaml
validations:
  - kind: bytes_field_matches_u8_length
    value_field: Metadata Length
    bytes_field: Metadata
```

## Authoring Rules

- Keep definitions declarative. If you need packet-specific logic, stop and extend the parser model instead.
- Preserve on-the-wire order exactly in `fields`.
- Prefer raw first-tier packet structure over speculative interpretation.
- Add top-level `name` and `description` even if the SIG lookup already has a name.
- Use stable human labels in `field`; these are shown directly in API and UI output.
- Use `type` to describe the semantic shape of the returned value, not the parser primitive.
- If a format includes a length byte and trailing payload, add an explicit validation.
- Use `enumerations` for named values and bit masks, not for narrative notes.

## Current Status Codes

The generic parser returns these codes:

- `200` `OK`
- `400` `Invalid Service UUID`
- `404` `Unknown Service Packet Format`
- `422` `Malformed Service Packet`
- `500` `Invalid Service Packet Definition`

## Current Limits

The schema currently supports only:

- fixed-width numeric fields
- a trailing remainder-bytes field
- exact-match and bit-mask enumeration expansion for numeric fields
- simple cross-field validations

If you need arrays, nested structures, repeated TLVs, bitfield expansion, or conditional fields, add those capabilities to the parser and generator first. Do not encode hacks into YAML to simulate them.
