import { Routes, Route } from "react-router-dom";
import { AppSidebar } from "@/components/layout/Sidebar";
import { StatusBar } from "@/components/layout/StatusBar";
import { SidebarInset, SidebarProvider } from "@/components/ui/sidebar";
import { Dashboard } from "@/pages/Dashboard";
import { Devices } from "@/pages/Devices";
import { StreamConfig } from "@/pages/StreamConfig";
import { Logs } from "@/pages/Logs";
import { useBluetoothAdapter } from "@/hooks/useBluetoothAdapter";
import { useDevices } from "@/hooks/useDevices";
import { useUpdater } from "@/hooks/useUpdater";

export default function App() {
  const bluetoothAdapter = useBluetoothAdapter();
  const { connectedDevices, bleDevices, scanning, startScan, stopScan } = useDevices();
  const updater = useUpdater();

  return (
    <SidebarProvider>
      <AppSidebar />
      <SidebarInset className="flex flex-col overflow-hidden">
        <main className="flex flex-1 flex-col overflow-hidden">
          <Routes>
            <Route
              path="/"
              element={
                <Dashboard
                  connectedCount={connectedDevices.length}
                  bluetoothAdapter={bluetoothAdapter}
                />
              }
            />
            <Route
              path="/devices"
              element={
                <Devices
                  bleDevices={bleDevices}
                  scanning={scanning}
                  onStartScan={startScan}
                  onStopScan={stopScan}
                />
              }
            />
            <Route path="/stream-config" element={<StreamConfig />} />
            <Route path="/logs" element={<Logs />} />
          </Routes>
        </main>
        <StatusBar
          connectedCount={connectedDevices.length}
          updater={updater}
          bluetoothAdapter={bluetoothAdapter}
        />
      </SidebarInset>
    </SidebarProvider>
  );
}
