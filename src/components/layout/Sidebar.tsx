import { NavLink } from "react-router-dom";
import {
  LayoutDashboard,
  Radio,
  Settings2,
  ScrollText,
} from "lucide-react";
import {
  Sidebar,
  SidebarContent,
  SidebarGroup,
  SidebarGroupContent,
  SidebarHeader,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
  SidebarSeparator,
} from "@/components/ui/sidebar";

const navItems = [
  { to: "/", icon: LayoutDashboard, label: "Dashboard" },
  { to: "/devices", icon: Radio, label: "Devices" },
  { to: "/stream-config", icon: Settings2, label: "Stream Config" },
  { to: "/logs", icon: ScrollText, label: "Logs" },
] as const;

export function AppSidebar() {
  return (
    <Sidebar collapsible="none">
      <SidebarHeader className="h-12 justify-center px-4">
        <span className="font-mono text-sm font-semibold tracking-tight">
          Auracle
        </span>
      </SidebarHeader>
      <SidebarSeparator />
      <SidebarContent>
        <SidebarGroup className="p-2">
          <SidebarGroupContent>
            <SidebarMenu>
              {navItems.map(({ to, icon: Icon, label }) => (
                <SidebarMenuItem key={to}>
                  <SidebarMenuButton asChild size="sm">
                    <NavLink to={to}>
                      <Icon size={14} />
                      <span>{label}</span>
                    </NavLink>
                  </SidebarMenuButton>
                </SidebarMenuItem>
              ))}
            </SidebarMenu>
          </SidebarGroupContent>
        </SidebarGroup>
      </SidebarContent>
    </Sidebar>
  );
}
