import { NavLink } from "react-router-dom";
import {
  Bluetooth,
  Search,
  Radio,
  ClipboardCheck,
  Sun,
  Moon,
  Palette,
  Headphones,
} from "lucide-react";
import type { LucideIcon } from "lucide-react";
import { useTheme } from "@/hooks/use-theme";
import { ICON_SIZE } from "@/lib/icons";

interface ActivityItem {
  to: string;
  icon: LucideIcon;
  label: string;
  end?: boolean;
}

const coreActivities: ActivityItem[] = [
  { to: "/", icon: Bluetooth, label: "Home", end: true },
  { to: "/generate", icon: Radio, label: "Generate" },
  { to: "/compliance", icon: ClipboardCheck, label: "Compliance" },
  { to: "/theme", icon: Palette, label: "Theme" },
];

interface CapabilityActivity extends ActivityItem {
  capability: string;
}

const capabilityActivities: CapabilityActivity[] = [
  { to: "/scan", icon: Search, label: "Scan", capability: "ble-scan" },
  { to: "/sink", icon: Headphones, label: "Sink", capability: "le-audio-sink" },
];

interface ActivityRailProps {
  activeCapabilities: string[];
}

function NavItem({ to, icon: Icon, label, end }: ActivityItem) {
  return (
    <NavLink
      to={to}
      end={end}
      className={({ isActive }) =>
        [
          "flex w-full flex-col items-center gap-1 rounded-md px-2 py-3 transition-colors",
          isActive
            ? "bg-secondary text-foreground"
            : "text-muted-foreground hover:bg-secondary/50",
        ].join(" ")
      }
    >
      <Icon size={ICON_SIZE.md} />
      <span className="text-xs font-medium">{label}</span>
    </NavLink>
  );
}

export function ActivityRail({ activeCapabilities }: ActivityRailProps) {
  const { theme, toggle } = useTheme();

  const visibleCapabilityItems = capabilityActivities.filter((item) =>
    activeCapabilities.includes(item.capability)
  );

  return (
    <nav className="flex h-full w-20 flex-col bg-background">
      <div className="flex flex-1 flex-col items-center gap-1 p-2">
        {coreActivities.map((item) => (
          <NavItem key={item.to} {...item} />
        ))}
        {visibleCapabilityItems.length > 0 && (
          <>
            <div className="my-1 h-px w-10 bg-border" />
            {visibleCapabilityItems.map((item) => (
              <NavItem key={item.to} {...item} />
            ))}
          </>
        )}
      </div>
      <div className="flex flex-col items-center p-2">
        <button
          onClick={toggle}
          className="flex w-full flex-col items-center gap-1 rounded-md px-2 py-3 text-muted-foreground transition-colors hover:bg-secondary/50"
          title={theme === "dark" ? "Switch to light mode" : "Switch to dark mode"}
        >
          {theme === "dark" ? <Sun size={ICON_SIZE.md} /> : <Moon size={ICON_SIZE.md} />}
          <span className="text-xs font-medium">Theme</span>
        </button>
      </div>
    </nav>
  );
}
