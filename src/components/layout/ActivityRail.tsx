import { NavLink } from "react-router-dom";
import { Bluetooth, Search, Radio, ClipboardCheck } from "lucide-react";
import type { LucideIcon } from "lucide-react";

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
];

export function ActivityRail() {
  return (
    <nav className="flex h-full w-[72px] flex-col border-r bg-background">
      <div className="flex flex-col items-center gap-1 p-2">
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
    </nav>
  );
}
