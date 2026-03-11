import bleCompanyIds from "@/data/ble-company-ids.json";

export const KNOWN_SERVICES: Record<string, string> = {
  "00001800-0000-1000-8000-00805f9b34fb": "Generic Access",
  "00001801-0000-1000-8000-00805f9b34fb": "Generic Attribute",
  "0000180a-0000-1000-8000-00805f9b34fb": "Device Information",
  "0000180d-0000-1000-8000-00805f9b34fb": "Heart Rate",
  "0000180f-0000-1000-8000-00805f9b34fb": "Battery Service",
  "0000184e-0000-1000-8000-00805f9b34fb": "Audio Stream Control",
  "0000184f-0000-1000-8000-00805f9b34fb": "Broadcast Audio Scan",
  "00001850-0000-1000-8000-00805f9b34fb": "Published Audio Capabilities",
  "00001851-0000-1000-8000-00805f9b34fb": "Basic Audio Profile",
  "3e1d50cd-7e3e-427d-8e1c-b78aa87fe624": "Focal Naim Auracast",
  "cee499e3-43a8-51d2-e7f4-1626ad235c0f": "Focal Naim Auracast",
};

const companyLookup = bleCompanyIds as Record<string, string>;

export interface CompanyOption {
  companyId: number;
  label: string;
}

export function resolveServiceName(uuid: string): string {
  return KNOWN_SERVICES[uuid.toLowerCase()] ?? uuid;
}

export function resolveCompanyName(id: number): string | null {
  return companyLookup[String(id)] ?? null;
}

export function searchCompanyOptions(query: string, limit = 50): CompanyOption[] {
  const normalizedQuery = query.trim().toLowerCase();

  return Object.entries(companyLookup)
    .map(([companyId, label]) => ({
      companyId: Number(companyId),
      label,
    }))
    .filter((option) => Number.isFinite(option.companyId))
    .filter((option) => {
      if (!normalizedQuery) {
        return true;
      }

      const hex = `0x${option.companyId.toString(16).padStart(4, "0")}`;
      return option.label.toLowerCase().includes(normalizedQuery)
        || option.companyId.toString().includes(normalizedQuery)
        || hex.includes(normalizedQuery);
    })
    .sort((a, b) => a.label.localeCompare(b.label))
    .slice(0, limit);
}

export function formatHexBytes(bytes: number[]): string {
  return bytes.map((b) => b.toString(16).padStart(2, "0").toUpperCase()).join(" ");
}

export function formatHexBlock(bytes: number[], groupSize = 2): string {
  if (bytes.length === 0) {
    return "No payload";
  }

  return bytes
    .map((byte) => byte.toString(16).padStart(2, "0").toUpperCase())
    .reduce<string[]>((groups, byte, index) => {
      const groupIndex = Math.floor(index / groupSize);
      groups[groupIndex] = groups[groupIndex] ? `${groups[groupIndex]}${byte}` : byte;
      return groups;
    }, [])
    .join(" ");
}

/** Consistent RSSI thresholds used by both SignalBars and numeric display */
export function rssiLevel(rssi: number): { bars: number; color: string; bgColor: string } {
  if (rssi > -50) return { bars: 4, color: "text-success", bgColor: "bg-success" };
  if (rssi > -60) return { bars: 3, color: "text-success", bgColor: "bg-success" };
  if (rssi > -70) return { bars: 2, color: "text-warning", bgColor: "bg-warning" };
  return { bars: 1, color: "text-destructive", bgColor: "bg-destructive" };
}

export function rssiColor(rssi: number): string {
  return rssiLevel(rssi).color;
}

export function relativeTime(isoString: string): string {
  const diff = Math.floor((Date.now() - new Date(isoString).getTime()) / 1000);
  if (diff < 1) return "now";
  if (diff < 60) return `${diff}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  return `${Math.floor(diff / 3600)}h ago`;
}

export function isStale(isoString: string): boolean {
  return Date.now() - new Date(isoString).getTime() > 30_000;
}

/** Returns true if the UUID matches the Bluetooth SIG base UUID pattern */
export function isStandardUuid(uuid: string): boolean {
  return /^0000[0-9a-f]{4}-0000-1000-8000-00805f9b34fb$/i.test(uuid);
}

function uuid16ToString(uuid: number): string {
  return `0000${uuid.toString(16).padStart(4, "0")}-0000-1000-8000-00805f9b34fb`;
}

function uuid32ToString(uuid: number): string {
  return `${uuid.toString(16).padStart(8, "0")}-0000-1000-8000-00805f9b34fb`;
}

function uuid128ToString(bytes: number[]): string {
  const reversed = [...bytes].reverse();
  const hex = reversed.map((b) => b.toString(16).padStart(2, "0")).join("");
  return [
    hex.slice(0, 8),
    hex.slice(8, 12),
    hex.slice(12, 16),
    hex.slice(16, 20),
    hex.slice(20, 32),
  ].join("-");
}

function decodeText(bytes: number[]): string {
  const trimmed = [...bytes];
  while (trimmed.length > 0 && trimmed[trimmed.length - 1] === 0) {
    trimmed.pop();
  }

  return new TextDecoder().decode(new Uint8Array(trimmed));
}

function littleEndian16(bytes: number[]): number {
  return (bytes[0] ?? 0) | ((bytes[1] ?? 0) << 8);
}

function littleEndian32(bytes: number[]): number {
  return (bytes[0] ?? 0)
    | ((bytes[1] ?? 0) << 8)
    | ((bytes[2] ?? 0) << 16)
    | ((bytes[3] ?? 0) << 24);
}

const AD_TYPE_NAMES: Record<number, string> = {
  0x01: "Flags",
  0x02: "Incomplete 16-bit UUIDs",
  0x03: "Complete 16-bit UUIDs",
  0x04: "Incomplete 32-bit UUIDs",
  0x05: "Complete 32-bit UUIDs",
  0x06: "Incomplete 128-bit UUIDs",
  0x07: "Complete 128-bit UUIDs",
  0x08: "Shortened Local Name",
  0x09: "Complete Local Name",
  0x0a: "Tx Power Level",
  0x16: "Service Data (16-bit)",
  0x19: "Appearance",
  0x20: "Service Data (32-bit)",
  0x21: "Service Data (128-bit)",
  0x30: "Broadcast Name",
  0xff: "Manufacturer Specific Data",
};

export interface AdvertisementStructure {
  offset: number;
  type: number;
  typeName: string;
  length: number;
  hex: string;
  summary: string;
}

export function parseAdvertisementStructures(bytes: number[]): AdvertisementStructure[] {
  const structures: AdvertisementStructure[] = [];
  let offset = 0;

  while (offset < bytes.length) {
    const fieldLength = bytes[offset];
    if (!fieldLength) {
      break;
    }

    if (offset + 1 + fieldLength > bytes.length) {
      structures.push({
        offset,
        type: -1,
        typeName: "Truncated Field",
        length: bytes.length - offset - 1,
        hex: formatHexBytes(bytes.slice(offset + 1)),
        summary: `Field length ${fieldLength} exceeds remaining payload`,
      });
      break;
    }

    const type = bytes[offset + 1] ?? 0;
    const field = bytes.slice(offset + 2, offset + 1 + fieldLength);
    const typeName = AD_TYPE_NAMES[type] ?? `Type 0x${type.toString(16).padStart(2, "0").toUpperCase()}`;
    let summary = field.length > 0 ? formatHexBytes(field) : "Empty";

    switch (type) {
      case 0x08:
      case 0x09:
      case 0x30:
        summary = decodeText(field) || "(empty text)";
        break;
      case 0x0a:
        summary = `${field.length > 0 ? (((field[0] ?? 0) << 24) >> 24) : 0} dBm`;
        break;
      case 0x02:
      case 0x03:
        summary = Array.from({ length: Math.floor(field.length / 2) }, (_, index) => {
          const uuid = uuid16ToString(littleEndian16(field.slice(index * 2, index * 2 + 2)));
          return resolveServiceName(uuid);
        }).join(", ");
        break;
      case 0x04:
      case 0x05:
        summary = Array.from({ length: Math.floor(field.length / 4) }, (_, index) => {
          const uuid = uuid32ToString(littleEndian32(field.slice(index * 4, index * 4 + 4)));
          return uuid;
        }).join(", ");
        break;
      case 0x06:
      case 0x07:
        summary = Array.from({ length: Math.floor(field.length / 16) }, (_, index) => {
          const uuid = uuid128ToString(field.slice(index * 16, index * 16 + 16));
          return resolveServiceName(uuid);
        }).join(", ");
        break;
      case 0x16:
        if (field.length >= 2) {
          const uuid = uuid16ToString(littleEndian16(field.slice(0, 2)));
          summary = `${resolveServiceName(uuid)} | ${formatHexBytes(field.slice(2))}`;
        }
        break;
      case 0x20:
        if (field.length >= 4) {
          const uuid = uuid32ToString(littleEndian32(field.slice(0, 4)));
          summary = `${uuid} | ${formatHexBytes(field.slice(4))}`;
        }
        break;
      case 0x21:
        if (field.length >= 16) {
          const uuid = uuid128ToString(field.slice(0, 16));
          summary = `${resolveServiceName(uuid)} | ${formatHexBytes(field.slice(16))}`;
        }
        break;
      case 0x19:
        if (field.length >= 2) {
          summary = `0x${littleEndian16(field.slice(0, 2)).toString(16).padStart(4, "0").toUpperCase()}`;
        }
        break;
      case 0xff:
        if (field.length >= 2) {
          const companyId = littleEndian16(field.slice(0, 2));
          const companyName = resolveCompanyName(companyId);
          summary = `${companyName ?? "Unknown Company"} (0x${companyId
            .toString(16)
            .padStart(4, "0")
            .toUpperCase()}) | ${formatHexBytes(field.slice(2))}`;
        }
        break;
      default:
        break;
    }

    structures.push({
      offset,
      type,
      typeName,
      length: field.length,
      hex: formatHexBytes(field),
      summary,
    });

    offset += fieldLength + 1;
  }

  return structures;
}

export function formatPacketTimestamp(timestampMs: number): string {
  const date = new Date(timestampMs);
  return `${date.toLocaleTimeString([], {
    hour12: false,
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  })}.${date.getMilliseconds().toString().padStart(3, "0")}`;
}

/** LE Audio service UUIDs — devices advertising any of these are broadcast audio sources */
export const LE_AUDIO_SERVICE_UUIDS: Set<string> = new Set([
  "0000184e-0000-1000-8000-00805f9b34fb", // Audio Stream Control
  "0000184f-0000-1000-8000-00805f9b34fb", // Broadcast Audio Scan (BASS)
  "00001850-0000-1000-8000-00805f9b34fb", // Published Audio Capabilities (PAC)
  "00001851-0000-1000-8000-00805f9b34fb", // Basic Audio Profile (BAP)
  "3e1d50cd-7e3e-427d-8e1c-b78aa87fe624", // Focal Naim Auracast
  "cee499e3-43a8-51d2-e7f4-1626ad235c0f", // Focal Naim Auracast
]);

/** Returns true if the device advertises any LE Audio service */
export function isLeAudioDevice(services: string[]): boolean {
  return services.some((s) => LE_AUDIO_SERVICE_UUIDS.has(s.toLowerCase()));
}
