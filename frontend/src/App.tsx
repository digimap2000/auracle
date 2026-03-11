import { useCallback, useEffect, useMemo, useState } from "react";
import { Routes, Route } from "react-router-dom";
import { ActivityRail } from "@/components/layout/ActivityRail";
import { StatusBar } from "@/components/layout/StatusBar";
import { ZoomIndicator } from "@/components/layout/ZoomIndicator";
import { Home } from "@/pages/Home";
import { Devices } from "@/pages/Devices";
import { Generators } from "@/pages/Generators";
import { Compliance } from "@/pages/Compliance";
import { Sink } from "@/pages/Sink";
import { ThemeReference } from "@/pages/ThemeReference";
import { useDevices } from "@/hooks/useDevices";
import { useUnits } from "@/hooks/useUnits";
import { useUpdater } from "@/hooks/useUpdater";
import { useZoom } from "@/hooks/use-zoom";

export default function App() {
  const [activeUnitId, setActiveUnitId] = useState<string | null>(null);
  const { units, loading: unitsLoading, error: unitsError, refresh: refreshUnits } = useUnits();
  const { connectedDevices, bleDevices, blePackets, scanning } = useDevices(activeUnitId, units);
  const updater = useUpdater();
  const { zoom } = useZoom();

  // Derive the full active unit object
  const activeUnit = useMemo(
    () => units.find((u) => u.id === activeUnitId) ?? null,
    [units, activeUnitId]
  );

  // Maintain a valid active unit selection, preferring the first scannable unit.
  useEffect(() => {
    const scannableUnits = units.filter(
      (unit) => unit.present && unit.capabilities.includes("ble-scan")
    );

    if (scannableUnits.length === 0) {
      if (activeUnitId !== null) {
        setActiveUnitId(null);
      }
      return;
    }

    if (!activeUnitId || !scannableUnits.some((unit) => unit.id === activeUnitId)) {
      const nextActiveUnit = scannableUnits[0];
      if (nextActiveUnit) {
        setActiveUnitId(nextActiveUnit.id);
      }
    }
  }, [units, activeUnitId]);

  const handleSelectUnit = useCallback((id: string) => {
    setActiveUnitId(id);
  }, []);

  const activeCapabilities = useMemo(
    () => activeUnit?.capabilities ?? [],
    [activeUnit]
  );

  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden bg-background">
      <ZoomIndicator zoom={zoom} />
      <div className="flex min-h-0 flex-1">
        <ActivityRail activeCapabilities={activeCapabilities} />
        <main className="flex flex-1 flex-col overflow-hidden rounded-tl-xl bg-background">
          <Routes>
            <Route
              path="/"
              element={
                <Home
                  units={units}
                  loading={unitsLoading}
                  error={unitsError}
                  refresh={refreshUnits}
                  activeUnitId={activeUnitId}
                  onSelectUnit={handleSelectUnit}
                />
              }
            />
            <Route
              path="/scan"
              element={
                <Devices
                  bleDevices={bleDevices}
                  blePackets={blePackets}
                  scanning={scanning}
                  activeUnit={activeUnit}
                />
              }
            />
            <Route
              path="/sink"
              element={
                <Sink
                  bleDevices={bleDevices}
                  scanning={scanning}
                  activeUnit={activeUnit}
                />
              }
            />
            <Route path="/generate" element={<Generators />} />
            <Route path="/compliance" element={<Compliance />} />
            <Route path="/theme" element={<ThemeReference />} />
          </Routes>
        </main>
      </div>
      <StatusBar
        activeUnit={activeUnit}
        connectedCount={connectedDevices.length}
        updater={updater}
      />
    </div>
  );
}
