import { ClipboardCheck } from "lucide-react";
import { Header } from "@/components/layout/Header";

export function Compliance() {
  return (
    <>
      <Header title="Compliance" description="LE Audio certification test runner" />
      <div className="flex flex-1 items-center justify-center">
        <div className="text-center text-muted-foreground">
          <ClipboardCheck className="mx-auto mb-3 size-8 opacity-30" />
          <p className="text-sm">Compliance test runner coming soon</p>
        </div>
      </div>
    </>
  );
}
