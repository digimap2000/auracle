import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

interface FormFieldProps {
  /** Field label (displayed in left column) */
  label: string;
  /** HTML id for the input — links the label's htmlFor */
  htmlFor?: string;
  /** Whether the field value is empty/default — dims the row */
  empty?: boolean;
  /** Align label to top for multiline controls (textarea) */
  multiline?: boolean;
  /** Optional className for the outer wrapper */
  className?: string;
  /** Called when the mouse enters the field row */
  onMouseEnter?: () => void;
  /** The form control (Input, Select, Textarea, etc.) */
  children: ReactNode;
}

export function FormField({
  label,
  htmlFor,
  empty,
  multiline,
  className,
  onMouseEnter,
  children,
}: FormFieldProps) {
  return (
    <div
      className={cn(
        "grid grid-cols-[1fr_2fr] gap-x-4 py-2.5",
        multiline ? "items-start" : "items-center",
        className,
      )}
      onMouseEnter={onMouseEnter}
    >
      <label
        htmlFor={htmlFor}
        className={cn(
          "text-right text-sm font-medium text-foreground",
          multiline && "pt-2",
        )}
      >
        {label}
      </label>
      <div
        className={cn(
          "transition-opacity",
          empty && "opacity-40 focus-within:opacity-100 hover:opacity-70"
        )}
      >
        {children}
      </div>
    </div>
  );
}
