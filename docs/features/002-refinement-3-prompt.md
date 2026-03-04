# Feature 002 — Refinement 3: Service UUID display improvements

Read CLAUDE.md, then apply these two changes to `src/pages/Devices.tsx`.

## Fix 1: Add custom service UUID for Focal Naim Auracast

Add the following entry to the `KNOWN_SERVICES` map:

```tsx
"3e1d50cd-7e3e-427d-8e1c-b78aa87fe624": "Focal Naim Auracast",
```

This is a custom service UUID used by Naim's Auracast broadcast implementation on the IDC777 Bluetooth module.

## Fix 2: Show UUID alongside resolved name in services list

In the `DeviceDetail` component's Services section, change the display so that **every service entry always shows the UUID in monospace**. When a UUID resolves to a known name, show the name as a badge alongside the UUID — not instead of it.

Currently the code shows either the name badge or the raw UUID. Change it so both are always visible:

```tsx
{device.services.map((uuid, i) => {
  const name = resolveServiceName(uuid);
  const isKnown = name !== uuid;
  return (
    <div key={i} className="flex items-center gap-2">
      {isKnown && (
        <Badge variant="outline" className="text-[10px]">{name}</Badge>
      )}
      <span className="font-mono text-[10px] text-muted-foreground">{uuid}</span>
    </div>
  );
})}
```

**Important**: Use the array index `i` as the React key (not the UUID), because the same UUID may appear multiple times. Do NOT deduplicate the services list — if a device advertises duplicates, that's meaningful information for firmware debugging.

## After fixing

1. `npm run build` — zero errors
2. Delete the source YAML file `sig.yml` from the project root — it's no longer needed
