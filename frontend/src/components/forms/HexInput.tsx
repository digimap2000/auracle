import { useCallback } from "react";
import { Input } from "@/components/ui/input";
import { cn } from "@/lib/utils";

interface HexInputProps {
  id?: string;
  value: string;
  onChange: (value: string) => void;
  onFocus?: () => void;
  placeholder?: string;
  className?: string;
}

/** Strip everything except hex digits */
function sanitiseHex(raw: string): string {
  return raw.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "");
}

/** Format hex string as uppercase space-separated byte pairs */
function formatHexDisplay(hex: string): string {
  const upper = hex.toUpperCase();
  const pairs: string[] = [];
  for (let i = 0; i < upper.length; i += 2) {
    pairs.push(upper.slice(i, i + 2));
  }
  return pairs.join(" ");
}

export function HexInput({
  id,
  value,
  onChange,
  onFocus,
  placeholder = "0A FF 12",
  className,
}: HexInputProps) {
  const handleChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const sanitised = sanitiseHex(e.target.value);
      onChange(sanitised);
    },
    [onChange]
  );

  const handleBlur = useCallback(() => {
    if (value) {
      onChange(sanitiseHex(value));
    }
  }, [value, onChange]);

  return (
    <Input
      id={id}
      value={formatHexDisplay(value)}
      onChange={handleChange}
      onFocus={onFocus}
      onBlur={handleBlur}
      placeholder={placeholder}
      className={cn("font-mono", className)}
    />
  );
}
