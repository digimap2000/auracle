import { Search } from "lucide-react";
import { Header } from "@/components/layout/Header";

export function Devices() {
  return (
    <div className="flex h-full flex-col">
      <Header title="Browse" description="Service browser" />
      <div className="flex flex-1 items-center justify-center">
        <div className="text-center text-muted-foreground">
          <Search className="mx-auto mb-3 size-8 opacity-30" />
          <p className="text-sm">Service browser coming soon</p>
        </div>
      </div>
    </div>
  );
}
