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

export function resolveServiceName(uuid: string): string {
  return KNOWN_SERVICES[uuid.toLowerCase()] ?? uuid;
}

export function resolveCompanyName(id: number): string | null {
  return companyLookup[String(id)] ?? null;
}

export function formatHexBytes(bytes: number[]): string {
  return bytes.map((b) => b.toString(16).padStart(2, "0").toUpperCase()).join(" ");
}

/** Consistent RSSI thresholds used by both SignalBars and numeric display */
export function rssiLevel(rssi: number): { bars: number; color: string; bgColor: string } {
  if (rssi > -50) return { bars: 4, color: "text-green-500", bgColor: "bg-green-500" };
  if (rssi > -60) return { bars: 3, color: "text-green-500", bgColor: "bg-green-500" };
  if (rssi > -70) return { bars: 2, color: "text-yellow-500", bgColor: "bg-yellow-500" };
  return { bars: 1, color: "text-red-500", bgColor: "bg-red-500" };
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
