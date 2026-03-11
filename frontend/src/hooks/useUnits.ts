import { useCallback, useEffect, useState } from "react";
import { type DaemonUnit, getDaemonUnits } from "@/lib/tauri";

const POLL_INTERVAL_MS = 5000;

interface UnitsState {
  units: DaemonUnit[];
  loading: boolean;
  error: string | null;
}

export function useUnits() {
  const [state, setState] = useState<UnitsState>({
    units: [],
    loading: true,
    error: null,
  });

  const refresh = useCallback(async () => {
    setState((prev) => ({ ...prev, loading: true, error: null }));
    try {
      const units = await getDaemonUnits();
      setState({ units, loading: false, error: null });
    } catch (err) {
      setState((prev) => ({
        ...prev,
        loading: false,
        error: err instanceof Error ? err.message : String(err),
      }));
    }
  }, []);

  // Fetch on mount + poll for changes
  useEffect(() => {
    refresh();
    const id = setInterval(() => {
      getDaemonUnits()
        .then((units) => setState({ units, loading: false, error: null }))
        .catch(() => {});
    }, POLL_INTERVAL_MS);
    return () => clearInterval(id);
  }, [refresh]);

  return { ...state, refresh };
}
