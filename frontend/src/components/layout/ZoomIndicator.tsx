import { useEffect, useRef, useState } from "react";

interface ZoomIndicatorProps {
  zoom: number;
}

export function ZoomIndicator({ zoom }: ZoomIndicatorProps) {
  const [visible, setVisible] = useState(false);
  const prevZoomRef = useRef(zoom);
  const timerRef = useRef<ReturnType<typeof setTimeout>>();

  useEffect(() => {
    // Only show when the value actually changes from the previous render
    if (zoom === prevZoomRef.current) return;
    prevZoomRef.current = zoom;

    setVisible(true);
    clearTimeout(timerRef.current);
    timerRef.current = setTimeout(() => setVisible(false), 800);

    return () => clearTimeout(timerRef.current);
  }, [zoom]);

  if (!visible) return null;

  return (
    <div className="pointer-events-none fixed inset-0 z-50 flex items-center justify-center">
      <div className="animate-in fade-in zoom-in-95 rounded-lg bg-foreground/80 px-5 py-3 text-background shadow-lg backdrop-blur-sm">
        <span className="text-lg font-semibold tabular-nums">
          {Math.round(zoom * 100)}%
        </span>
      </div>
    </div>
  );
}
