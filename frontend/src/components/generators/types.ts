/** Identity configuration for a device definition */
export interface DeviceIdentity {
  /** BLE advertised name (Complete Local Name) */
  advertisedName: string;
  /** Bluetooth SIG company ID for manufacturer data */
  manufacturerId: string;
  /** Raw manufacturer-specific data as hex string (no spaces) */
  manufacturerData: string;
  /** GAP Appearance value (16-bit code) */
  appearanceCode: string;
  /** Device description (human-readable note, not broadcast) */
  description: string;
}

export const DEFAULT_DEVICE_IDENTITY: DeviceIdentity = {
  advertisedName: "",
  manufacturerId: "",
  manufacturerData: "",
  appearanceCode: "",
  description: "",
};

/** Curated Bluetooth SIG company identifiers relevant to Auracast development */
export const MANUFACTURER_OPTIONS = [
  { id: "0x029A", name: "Focal Naim" },
  { id: "0x0BA4", name: "Vervent Audio Group" },
  { id: "0x0059", name: "Nordic Semiconductor" },
  { id: "0x000A", name: "Qualcomm" },
  { id: "0x004C", name: "Apple" },
  { id: "0x0006", name: "Microsoft" },
  { id: "0x00E0", name: "Google" },
  { id: "0x0075", name: "Samsung" },
  { id: "0x000D", name: "Texas Instruments" },
  { id: "0x0046", name: "MediaTek" },
] as const;

/** Curated GAP Appearance values for LE Audio devices */
export const APPEARANCE_OPTIONS = [
  { code: "0x0000", label: "Unknown" },
  { code: "0x0041", label: "Generic Audio Source" },
  { code: "0x0042", label: "Standalone Speaker" },
  { code: "0x0043", label: "Bookshelf Speaker" },
  { code: "0x0044", label: "Soundbar" },
  { code: "0x0045", label: "Subwoofer" },
  { code: "0x0841", label: "Headphones" },
  { code: "0x0842", label: "In-ear Headphones" },
  { code: "0x0843", label: "Earbuds" },
  { code: "0x0941", label: "Generic Hearing Aid" },
  { code: "0x0942", label: "Behind-ear Hearing Aid" },
  { code: "0x0943", label: "In-ear Hearing Aid" },
  { code: "0x0A41", label: "Generic Microphone" },
  { code: "0x0B41", label: "Generic Broadcast Device" },
] as const;
