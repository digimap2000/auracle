import { useMemo } from "react";
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
  const { units, loading: unitsLoading, error: unitsError, refresh: refreshUnits } = useUnits();
  const { connectedDevices, allBleDevices, allBlePackets, scanning } = useDevices(units);
  const updater = useUpdater();
  const { zoom } = useZoom();

  const availableCapabilities = useMemo(
    () => [...new Set(
      units
        .filter((unit) => unit.present)
        .flatMap((unit) => unit.capabilities)
    )].sort(),
    [units]
  );

  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden bg-background">
      <ZoomIndicator zoom={zoom} />
      <div className="flex min-h-0 flex-1">
        <ActivityRail activeCapabilities={availableCapabilities} />
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
                />
              }
            />
            <Route
              path="/scan"
              element={
                <Devices
                  units={units}
                  bleDevices={allBleDevices}
                  blePackets={allBlePackets}
                  scanning={scanning}
                />
              }
            />
            <Route
              path="/sink"
              element={
                <Sink
                  units={units}
                  bleDevices={allBleDevices}
                  scanning={scanning}
                />
              }
            />
            <Route path="/generate" element={<Generators />} />
            <Route path="/compliance" element={<Compliance units={units} bleDevices={allBleDevices} />} />
            <Route path="/theme" element={<ThemeReference />} />
          </Routes>
        </main>
      </div>
      <StatusBar
        units={units}
        connectedCount={connectedDevices.length}
        updater={updater}
      />
    </div>
  );
}
