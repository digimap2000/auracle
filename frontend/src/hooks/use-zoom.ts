import { useCallback, useEffect, useState } from "react";

const STEP = 0.1;
const MIN = 0.5;
const MAX = 2.0;
const STORAGE_KEY = "auracle-zoom";

function getInitialZoom(): number {
  const stored = localStorage.getItem(STORAGE_KEY);
  if (stored) {
    const parsed = parseFloat(stored);
    if (!isNaN(parsed) && parsed >= MIN && parsed <= MAX) return parsed;
  }
  return 1;
}

function applyZoom(level: number) {
  document.documentElement.style.zoom = String(level);
}

export function useZoom() {
  const [zoom, setZoom] = useState(getInitialZoom);

  useEffect(() => {
    applyZoom(zoom);
    localStorage.setItem(STORAGE_KEY, String(zoom));
  }, [zoom]);

  const zoomIn = useCallback(() => {
    setZoom((prev) => Math.min(MAX, Math.round((prev + STEP) * 10) / 10));
  }, []);

  const zoomOut = useCallback(() => {
    setZoom((prev) => Math.max(MIN, Math.round((prev - STEP) * 10) / 10));
  }, []);

  const resetZoom = useCallback(() => {
    setZoom(1);
  }, []);

  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      const mod = e.metaKey || e.ctrlKey;
      if (!mod) return;

      if (e.key === "=" || e.key === "+") {
        e.preventDefault();
        zoomIn();
      } else if (e.key === "-") {
        e.preventDefault();
        zoomOut();
      } else if (e.key === "0") {
        e.preventDefault();
        resetZoom();
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [zoomIn, zoomOut, resetZoom]);

  return { zoom, zoomIn, zoomOut, resetZoom } as const;
}
