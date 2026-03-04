import { NavLink } from "react-router-dom";
import {
  LayoutDashboard,
  Radio,
  Settings2,
  ScrollText,
} from "lucide-react";
import { cn } from "@/lib/utils";
import { Separator } from "@/components/ui/separator";

const navItems = [
  { to: "/", icon: LayoutDashboard, label: "Dashboard" },
  { to: "/devices", icon: Radio, label: "Devices" },
  { to: "/stream-config", icon: Settings2, label: "Stream Config" },
  { to: "/logs", icon: ScrollText, label: "Logs" },
] as const;

export function Sidebar() {
  return (
    <aside className="flex h-full w-[220px] shrink-0 flex-col border-r bg-background">
      <div className="flex h-12 items-center px-4">
        <span className="font-mono text-sm font-semibold tracking-tight text-foreground">
          Auracle
        </span>
      </div>
      <Separator />
      <nav className="flex flex-col gap-0.5 p-2">
        {navItems.map(({ to, icon: Icon, label }) => (
          <NavLink
            key={to}
            to={to}
            className={({ isActive }) =>
              cn(
                "flex items-center gap-2.5 rounded-md px-2.5 py-1.5 text-[13px] font-medium transition-colors",
                isActive
                  ? "bg-secondary text-foreground"
                  : "text-muted-foreground hover:bg-secondary/50 hover:text-foreground"
              )
            }
          >
            <Icon size={14} />
            {label}
          </NavLink>
        ))}
      </nav>
    </aside>
  );
}
