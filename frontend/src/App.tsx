import { Routes, Route } from "react-router-dom";
import { ActivityRail } from "@/components/layout/ActivityRail";
import { StatusBar } from "@/components/layout/StatusBar";
import { ZoomIndicator } from "@/components/layout/ZoomIndicator";
import { Home } from "@/pages/Home";
import { Devices } from "@/pages/Devices";
import { Generators } from "@/pages/Generators";
import { Compliance } from "@/pages/Compliance";
import { ThemeReference } from "@/pages/ThemeReference";
import { useBluetoothAdapter } from "@/hooks/useBluetoothAdapter";
import { useDevices } from "@/hooks/useDevices";
import { useUpdater } from "@/hooks/useUpdater";
import { useZoom } from "@/hooks/use-zoom";

export default function App() {
  const bluetoothAdapter = useBluetoothAdapter();
  const { connectedDevices, bleDevices, scanning, startScan, stopScan } = useDevices();
  const updater = useUpdater();
  const { zoom } = useZoom();

  return (
    <div className="flex h-screen w-screen overflow-hidden">
      <ZoomIndicator zoom={zoom} />
      <ActivityRail />
      <div className="flex flex-1 flex-col overflow-hidden">
        <main className="flex flex-1 flex-col overflow-hidden">
          <Routes>
            <Route
              path="/"
              element={
                <Home
                  bleDevices={bleDevices}
                  scanning={scanning}
                  onStartScan={startScan}
                  onStopScan={stopScan}
                />
              }
            />
            <Route path="/browse" element={<Devices />} />
            <Route path="/generate" element={<Generators />} />
            <Route path="/compliance" element={<Compliance />} />
            <Route path="/theme" element={<ThemeReference />} />
          </Routes>
        </main>
        <StatusBar
          connectedCount={connectedDevices.length}
          updater={updater}
          bluetoothAdapter={bluetoothAdapter}
        />
      </div>
    </div>
  );
}
