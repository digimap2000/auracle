interface HeaderProps {
  title: string;
  description?: string;
}

export function Header({ title, description }: HeaderProps) {
  return (
    <div className="flex h-12 shrink-0 items-center border-b px-4">
      <div className="flex flex-col gap-0">
        <h1 className="text-sm font-medium text-foreground">{title}</h1>
        {description && (
          <p className="text-[11px] text-muted-foreground">{description}</p>
        )}
      </div>
    </div>
  );
}
