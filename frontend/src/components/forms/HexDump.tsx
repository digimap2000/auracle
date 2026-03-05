import { useMemo } from "react";

interface HexDumpProps {
  /** Raw hex string (no spaces, e.g. "0AFF12B4") */
  data: string;
  /** Bytes per line — must be a multiple of 4, defaults to 16 */
  bytesPerLine?: number;
}

interface DumpLine {
  offset: string;
  hex: string;
  ascii: string;
}

function buildLines(hex: string, bytesPerLine: number): DumpLine[] {
  const upper = hex.replace(/[^0-9a-fA-F]/g, "").toUpperCase();
  const charsPerLine = bytesPerLine * 2;
  const lines: DumpLine[] = [];

  // Pre-calculate the full-width hex column length for padding.
  // Format: groups of 4 bytes ("XX XX XX XX") separated by double spaces.
  // Each group of 4 = 11 chars, separator between groups = 2 chars.
  const groupCount = Math.ceil(bytesPerLine / 4);
  const fullHexWidth = groupCount * 11 + Math.max(0, groupCount - 1) * 2;

  for (let i = 0; i < upper.length; i += charsPerLine) {
    const chunk = upper.slice(i, i + charsPerLine);
    const byteCount = Math.floor(chunk.length / 2);

    // Build hex pairs
    const pairs: string[] = [];
    for (let j = 0; j < chunk.length; j += 2) {
      pairs.push(chunk.slice(j, j + 2));
    }

    // Group into sets of 4 with double-space separator
    const grouped: string[] = [];
    for (let g = 0; g < pairs.length; g += 4) {
      grouped.push(pairs.slice(g, g + 4).join(" "));
    }
    const hexStr = grouped.join("  ").padEnd(fullHexWidth);

    // ASCII: printable chars or dot for non-printable
    let ascii = "";
    for (let j = 0; j < byteCount; j++) {
      const byte = parseInt(chunk.slice(j * 2, j * 2 + 2), 16);
      ascii += byte >= 0x20 && byte <= 0x7e ? String.fromCharCode(byte) : ".";
    }

    const offset = (i / 2).toString(16).toUpperCase().padStart(4, "0");
    lines.push({ offset, hex: hexStr, ascii });
  }

  return lines;
}

export function HexDump({ data, bytesPerLine = 16 }: HexDumpProps) {
  const lines = useMemo(() => buildLines(data, bytesPerLine), [data, bytesPerLine]);
  const byteCount = Math.floor(data.replace(/[^0-9a-fA-F]/g, "").length / 2);

  if (lines.length === 0) return null;

  return (
    <div className="space-y-1">
      <div className="overflow-x-auto rounded-md bg-secondary/50 px-3 py-2">
        <pre className="font-mono text-[11px] leading-relaxed">
          {lines.map((line) => (
            <div key={line.offset} className="flex">
              <span className="text-muted-foreground/50">{line.offset}</span>
              <span className="mx-3 text-foreground/80 whitespace-pre">{line.hex}</span>
              <span className="text-muted-foreground">{line.ascii}</span>
            </div>
          ))}
        </pre>
      </div>
      <p className="text-right text-[10px] text-muted-foreground/50">
        {byteCount} {byteCount === 1 ? "byte" : "bytes"}
      </p>
    </div>
  );
}
