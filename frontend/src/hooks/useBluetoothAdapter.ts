import { useCallback, useEffect, useState } from "react";
import { type BluetoothAdapter, getBluetoothAdapter } from "@/lib/tauri";

interface BluetoothAdapterState {
  adapter: BluetoothAdapter | null;
  loading: boolean;
  error: string | null;
}

export function useBluetoothAdapter() {
  const [state, setState] = useState<BluetoothAdapterState>({
    adapter: null,
    loading: true,
    error: null,
  });

  const refresh = useCallback(async () => {
    setState((prev) => ({ ...prev, loading: true, error: null }));
    try {
      const adapter = await getBluetoothAdapter();
      setState({ adapter, loading: false, error: null });
    } catch (err) {
      setState({
        adapter: null,
        loading: false,
        error: err instanceof Error ? err.message : String(err),
      });
    }
  }, []);

  useEffect(() => {
    refresh();
  }, [refresh]);

  return { ...state, refresh };
}
