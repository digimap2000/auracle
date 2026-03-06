import { useState, useCallback, useMemo } from "react";
import { Binary, Type } from "lucide-react";
import { Input } from "@/components/ui/input";
import { Button } from "@/components/ui/button";
import {
  Tooltip,
  TooltipContent,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { cn } from "@/lib/utils";

type Mode = "hex" | "text";

interface DataInputProps {
  id?: string;
  /** Raw hex string (no spaces, e.g. "48656C6C6F") */
  value: string;
  /** Called with the raw hex string */
  onChange: (value: string) => void;
  placeholder?: string;
  className?: string;
}

// ── Conversion helpers ──────────────────────────────────────────────

function sanitiseHex(raw: string): string {
  return raw.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "");
}

function formatHexDisplay(hex: string): string {
  const upper = hex.toUpperCase();
  const pairs: string[] = [];
  for (let i = 0; i < upper.length; i += 2) {
    pairs.push(upper.slice(i, i + 2));
  }
  return pairs.join(" ");
}

function hexToText(hex: string): string {
  let text = "";
  for (let i = 0; i < hex.length; i += 2) {
    const byte = parseInt(hex.slice(i, i + 2), 16);
    text += byte >= 0x20 && byte <= 0x7e ? String.fromCharCode(byte) : "\ufffd";
  }
  return text;
}

function textToHex(text: string): string {
  let hex = "";
  for (let i = 0; i < text.length; i++) {
    const code = text.charCodeAt(i);
    // Only encode printable ASCII; drop replacement characters
    if (code >= 0x20 && code <= 0x7e) {
      hex += code.toString(16).padStart(2, "0");
    }
  }
  return hex;
}

// ── Component ───────────────────────────────────────────────────────

export function DataInput({
  id,
  value,
  onChange,
  placeholder,
  className,
}: DataInputProps) {
  const [mode, setMode] = useState<Mode>("hex");

  const displayValue = useMemo(
    () => (mode === "hex" ? formatHexDisplay(value) : hexToText(value)),
    [mode, value]
  );

  const handleChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const raw = e.target.value;

      if (mode === "hex") {
        // Auto-switch to text if the very first character typed is non-hex
        if (!value && raw.length === 1 && !/^[0-9a-fA-F]$/.test(raw)) {
          setMode("text");
          onChange(textToHex(raw));
          return;
        }
        onChange(sanitiseHex(raw));
      } else {
        onChange(textToHex(raw));
      }
    },
    [mode, value, onChange]
  );

  const handleBlur = useCallback(() => {
    if (mode === "hex" && value) {
      onChange(sanitiseHex(value));
    }
  }, [mode, value, onChange]);

  const hexPlaceholder = placeholder ?? "0A FF 12 B4";
  const textPlaceholder = "Hello";

  return (
    <div className={cn("flex gap-1", className)}>
      <Input
        id={id}
        value={displayValue}
        onChange={handleChange}
        onBlur={handleBlur}
        placeholder={mode === "hex" ? hexPlaceholder : textPlaceholder}
        className={cn(mode === "hex" && "font-mono")}
      />
      <div className="flex shrink-0 items-center rounded-md border bg-secondary/50 p-0.5">
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              type="button"
              variant={mode === "hex" ? "secondary" : "ghost"}
              size="icon-xs"
              onClick={() => setMode("hex")}
              aria-label="Hex mode"
            >
              <Binary />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" className="text-xs">
            Hex
          </TooltipContent>
        </Tooltip>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              type="button"
              variant={mode === "text" ? "secondary" : "ghost"}
              size="icon-xs"
              onClick={() => setMode("text")}
              aria-label="Text mode"
            >
              <Type />
            </Button>
          </TooltipTrigger>
          <TooltipContent side="bottom" className="text-xs">
            Text
          </TooltipContent>
        </Tooltip>
      </div>
    </div>
  );
}
