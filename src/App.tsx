import { Routes, Route } from "react-router-dom";
import { Sidebar } from "@/components/layout/Sidebar";
import { StatusBar } from "@/components/layout/StatusBar";
import { Dashboard } from "@/pages/Dashboard";
import { Devices } from "@/pages/Devices";
import { StreamConfig } from "@/pages/StreamConfig";
import { Logs } from "@/pages/Logs";
import { useDevices } from "@/hooks/useDevices";
import { useUpdater } from "@/hooks/useUpdater";

export default function App() {
  const { connectedDevices, scanning, scan } = useDevices();
  const updater = useUpdater();

  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden bg-background">
      <div className="flex flex-1 overflow-hidden">
        <Sidebar />
        <main className="flex flex-1 flex-col overflow-hidden">
          <Routes>
            <Route
              path="/"
              element={
                <Dashboard connectedCount={connectedDevices.length} />
              }
            />
            <Route
              path="/devices"
              element={<Devices scanning={scanning} onScan={scan} />}
            />
            <Route path="/stream-config" element={<StreamConfig />} />
            <Route path="/logs" element={<Logs />} />
          </Routes>
        </main>
      </div>
      <StatusBar connectedCount={connectedDevices.length} updater={updater} />
    </div>
  );
}
