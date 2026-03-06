import { useRef, useLayoutEffect, useState } from "react";
import { cn } from "@/lib/utils";

/**
 * Pixel-snapped text label for flex `items-center` containers.
 *
 * At non-default root font sizes (e.g. 14px), Tailwind's rem-based
 * line-heights often resolve to fractional pixels (text-sm → 17.5px).
 * When `items-center` splits the remaining space, the half-pixel
 * offset is rounded by the browser, visibly shifting text off-center.
 *
 * CapLabel measures its computed line-height on mount and rounds it
 * to the nearest whole pixel, ensuring the centering math produces
 * integer offsets. The outer box size is unchanged — parents see the
 * same layout as a plain `<span>`.
 */

interface CapLabelProps extends React.HTMLAttributes<HTMLSpanElement> {
  children: React.ReactNode;
}

function CapLabel({ className, style, children, ...props }: CapLabelProps) {
  const ref = useRef<HTMLSpanElement>(null);
  const [snappedLH, setSnappedLH] = useState<number | null>(null);

  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const computed = parseFloat(getComputedStyle(el).lineHeight);
    if (!isNaN(computed)) {
      const rounded = Math.round(computed);
      if (rounded !== computed) setSnappedLH(rounded);
    }
  }, []);

  return (
    <span
      ref={ref}
      className={cn("truncate", className)}
      style={snappedLH != null ? { lineHeight: `${snappedLH}px`, ...style } : style}
      {...props}
    >
      {children}
    </span>
  );
}

export { CapLabel };
