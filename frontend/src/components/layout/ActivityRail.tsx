import { NavLink } from "react-router-dom";
import { Bluetooth, Search, Radio, ClipboardCheck, Sun, Moon, Palette } from "lucide-react";
import type { LucideIcon } from "lucide-react";
import { useTheme } from "@/hooks/use-theme";

interface ActivityItem {
  to: string;
  icon: LucideIcon;
  label: string;
  end?: boolean;
}

const activities: ActivityItem[] = [
  { to: "/", icon: Bluetooth, label: "Home", end: true },
  { to: "/browse", icon: Search, label: "Browse" },
  { to: "/generate", icon: Radio, label: "Generate" },
  { to: "/compliance", icon: ClipboardCheck, label: "Compliance" },
  { to: "/theme", icon: Palette, label: "Theme" },
];

export function ActivityRail() {
  const { theme, toggle } = useTheme();

  return (
    <nav className="flex h-full w-[72px] flex-col border-r bg-background">
      <div className="flex flex-1 flex-col items-center gap-1 p-2">
        {activities.map(({ to, icon: Icon, label, end }) => (
          <NavLink
            key={to}
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
            <Icon size={16} />
            <span className="text-[10px] font-medium">{label}</span>
          </NavLink>
        ))}
      </div>
      <div className="flex flex-col items-center p-2">
        <button
          onClick={toggle}
          className="flex w-full flex-col items-center gap-1 rounded-md px-2 py-3 text-muted-foreground transition-colors hover:bg-secondary/50"
          title={theme === "dark" ? "Switch to light mode" : "Switch to dark mode"}
        >
          {theme === "dark" ? <Sun size={16} /> : <Moon size={16} />}
          <span className="text-[10px] font-medium">Theme</span>
        </button>
      </div>
    </nav>
  );
}
