import { useEffect, useMemo, useState } from "react";
import { ClipboardCheck, Loader2, RefreshCw, RadioTower, TriangleAlert } from "lucide-react";
import { Header } from "@/components/layout/Header";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { useCompliance } from "@/hooks/useCompliance";
import type { BleDevice, ComplianceFinding, DaemonUnit } from "@/lib/tauri";

interface ComplianceProps {
  units: DaemonUnit[];
  bleDevices: BleDevice[];
}

interface ObservedDeviceOption {
  id: string;
  name: string;
  stableId: string;
  services: string[];
  lastSeen: string;
}

function canRunCompliance(unit: DaemonUnit) {
  return unit.present && unit.capabilities.includes("ble-scan");
}

function verdictVariant(verdict: string): "error" | "warn" | "info" | "outline" {
  switch (verdict) {
    case "FAIL":
      return "error";
    case "WARN":
      return "warn";
    case "INFO":
      return "info";
    default:
      return "outline";
  }
}

function countVerdicts(findings: ComplianceFinding[]) {
  return findings.reduce(
    (counts, finding) => {
      if (finding.verdict === "FAIL") counts.fail += 1;
      else if (finding.verdict === "WARN") counts.warn += 1;
      else if (finding.verdict === "INFO") counts.info += 1;
      return counts;
    },
    { fail: 0, warn: 0, info: 0 }
  );
}

export function Compliance({ units, bleDevices }: ComplianceProps) {
  const { rules, suites, result, loading, running, error, refresh, runRule, runSuite } = useCompliance();

  const scannerUnits = useMemo(
    () => units.filter(canRunCompliance),
    [units]
  );

  const [selectedScannerUnitId, setSelectedScannerUnitId] = useState<string | null>(null);
  const [selectedObservedDeviceId, setSelectedObservedDeviceId] = useState<string | null>(null);
  const [selectedRuleId, setSelectedRuleId] = useState<string | null>(null);
  const [selectedSuiteId, setSelectedSuiteId] = useState<string | null>(null);

  useEffect(() => {
    if (scannerUnits.length === 0) {
      setSelectedScannerUnitId(null);
      return;
    }
    if (!selectedScannerUnitId || !scannerUnits.some((unit) => unit.id === selectedScannerUnitId)) {
      setSelectedScannerUnitId(scannerUnits[0]?.id ?? null);
    }
  }, [scannerUnits, selectedScannerUnitId]);

  const observedDevices = useMemo<ObservedDeviceOption[]>(() => {
    if (!selectedScannerUnitId) {
      return [];
    }

    return bleDevices
      .filter((device) => device.unit_id === selectedScannerUnitId)
      .map((device) => ({
        id: device.id,
        name: device.name,
        stableId: device.stable_id,
        services: device.services,
        lastSeen: device.last_seen,
      }))
      .sort((a, b) => {
        const aUnknown = a.name === "Unknown";
        const bUnknown = b.name === "Unknown";
        if (aUnknown !== bUnknown) {
          return aUnknown ? 1 : -1;
        }
        if (a.name !== b.name) {
          return a.name.localeCompare(b.name);
        }
        return a.id.localeCompare(b.id);
      });
  }, [bleDevices, selectedScannerUnitId]);

  useEffect(() => {
    if (observedDevices.length === 0) {
      setSelectedObservedDeviceId(null);
      return;
    }
    if (!selectedObservedDeviceId || !observedDevices.some((device) => device.id === selectedObservedDeviceId)) {
      setSelectedObservedDeviceId(observedDevices[0]?.id ?? null);
    }
  }, [observedDevices, selectedObservedDeviceId]);

  useEffect(() => {
    if (rules.length > 0 && (!selectedRuleId || !rules.some((rule) => rule.id === selectedRuleId))) {
      setSelectedRuleId(rules[0]?.id ?? null);
    }
  }, [rules, selectedRuleId]);

  useEffect(() => {
    if (suites.length > 0 && (!selectedSuiteId || !suites.some((suite) => suite.id === selectedSuiteId))) {
      setSelectedSuiteId(suites[0]?.id ?? null);
    }
  }, [suites, selectedSuiteId]);

  const selectedRule = useMemo(
    () => rules.find((rule) => rule.id === selectedRuleId) ?? null,
    [rules, selectedRuleId]
  );
  const selectedSuite = useMemo(
    () => suites.find((suite) => suite.id === selectedSuiteId) ?? null,
    [suites, selectedSuiteId]
  );
  const selectedScannerUnit = useMemo(
    () => scannerUnits.find((unit) => unit.id === selectedScannerUnitId) ?? null,
    [scannerUnits, selectedScannerUnitId]
  );
  const selectedObservedDevice = useMemo(
    () => observedDevices.find((device) => device.id === selectedObservedDeviceId) ?? null,
    [observedDevices, selectedObservedDeviceId]
  );

  const counts = useMemo(
    () => countVerdicts(result?.findings ?? []),
    [result]
  );

  return (
    <>
      <Header title="Compliance" description="Run daemon-backed Auracast compliance rules against one detected DUT using a selected scanner unit." />
      <div className="flex min-h-0 flex-1 flex-col">
        <ScrollArea className="h-full">
          <div className="mx-auto flex w-full max-w-6xl flex-col gap-4 px-6 py-6">
            <div className="grid gap-4 lg:grid-cols-2">
              <Card>
                <CardHeader>
                  <CardTitle>Scanner Unit</CardTitle>
                  <CardDescription>
                    Choose the hardware unit that is sniffing the radio environment and retaining BLE observations.
                  </CardDescription>
                </CardHeader>
                <CardContent className="flex flex-col gap-3 sm:flex-row sm:items-center">
                  <Select value={selectedScannerUnitId ?? undefined} onValueChange={setSelectedScannerUnitId}>
                    <SelectTrigger className="w-full bg-background sm:min-w-80">
                      <SelectValue placeholder="Select scanner unit" />
                    </SelectTrigger>
                    <SelectContent>
                      {scannerUnits.map((unit) => (
                        <SelectItem key={unit.id} value={unit.id}>
                          {(unit.product || unit.kind) + " · " + unit.id}
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                  <Button variant="outline" onClick={refresh} disabled={loading || running}>
                    <RefreshCw className={loading ? "animate-spin" : ""} />
                    Refresh Catalog
                  </Button>
                </CardContent>
              </Card>

              <Card>
                <CardHeader>
                  <CardTitle>Detected Device</CardTitle>
                  <CardDescription>
                    Select the DUT found by the chosen scanner unit. Compliance runs target exactly one observed device.
                  </CardDescription>
                </CardHeader>
                <CardContent className="space-y-3">
                  <Select
                    value={selectedObservedDeviceId ?? undefined}
                    onValueChange={setSelectedObservedDeviceId}
                    disabled={observedDevices.length === 0}
                  >
                    <SelectTrigger className="w-full bg-background">
                      <SelectValue placeholder="Select observed DUT" />
                    </SelectTrigger>
                    <SelectContent>
                      {observedDevices.map((device) => (
                        <SelectItem key={device.id} value={device.id}>
                          {(device.name !== "Unknown" ? device.name : device.id) + " · " + device.id}
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                  {selectedObservedDevice ? (
                    <div className="rounded-md border bg-background p-3 text-xs text-muted-foreground">
                      <div className="flex flex-wrap items-center gap-2">
                        <p className="font-medium text-foreground">
                          {selectedObservedDevice.name !== "Unknown"
                            ? selectedObservedDevice.name
                            : selectedObservedDevice.id}
                        </p>
                        <Badge variant="outline" className="font-mono">{selectedObservedDevice.id}</Badge>
                      </div>
                      <p className="mt-1 font-mono">{selectedObservedDevice.stableId}</p>
                      <p className="mt-2">Last seen: {selectedObservedDevice.lastSeen}</p>
                      <p className="mt-1">
                        {selectedObservedDevice.services.length > 0
                          ? `${selectedObservedDevice.services.length} advertised services`
                          : "No decoded service UUIDs on the retained snapshot"}
                      </p>
                    </div>
                  ) : (
                    <div className="rounded-md border border-dashed bg-background p-3 text-xs text-muted-foreground">
                      Run a scan on the Devices page and wait for the selected scanner to discover the DUT.
                    </div>
                  )}
                </CardContent>
              </Card>
            </div>

            {error && (
              <Card>
                <CardContent className="flex items-center gap-3 pt-4 text-destructive">
                  <TriangleAlert className="size-4 shrink-0" />
                  <p className="text-sm">{error}</p>
                </CardContent>
              </Card>
            )}

            {loading ? (
              <Card>
                <CardContent className="flex items-center gap-3 pt-4 text-muted-foreground">
                  <Loader2 className="size-4 animate-spin" />
                  <p className="text-sm">Loading compliance catalog from the daemon...</p>
                </CardContent>
              </Card>
            ) : scannerUnits.length === 0 ? (
              <Card>
                <CardContent className="flex items-center gap-3 pt-4 text-muted-foreground">
                  <ClipboardCheck className="size-4 shrink-0 opacity-50" />
                  <p className="text-sm">No present units advertise BLE scan capability yet.</p>
                </CardContent>
              </Card>
            ) : observedDevices.length === 0 ? (
              <Card>
                <CardContent className="flex items-center gap-3 pt-4 text-muted-foreground">
                  <RadioTower className="size-4 shrink-0 opacity-50" />
                  <p className="text-sm">The selected scanner unit has not retained any detected DUTs yet.</p>
                </CardContent>
              </Card>
            ) : (
              <>
                <div className="grid gap-4 lg:grid-cols-2">
                  <Card>
                    <CardHeader>
                      <CardTitle>Run Suite</CardTitle>
                      <CardDescription>
                        Execute a curated compliance suite against the selected DUT only.
                      </CardDescription>
                    </CardHeader>
                    <CardContent className="space-y-3">
                      <Select value={selectedSuiteId ?? undefined} onValueChange={setSelectedSuiteId}>
                        <SelectTrigger className="w-full bg-background">
                          <SelectValue placeholder="Select suite" />
                        </SelectTrigger>
                        <SelectContent>
                          {suites.map((suite) => (
                            <SelectItem key={suite.id} value={suite.id}>
                              {(suite.title || suite.id) + " · " + suite.rule_ids.length + " rules"}
                            </SelectItem>
                          ))}
                        </SelectContent>
                      </Select>
                      {selectedSuite && (
                        <div className="rounded-md border bg-background p-3 text-xs text-muted-foreground">
                          <p className="font-medium text-foreground">{selectedSuite.title || selectedSuite.id}</p>
                          <p className="mt-1">{selectedSuite.rule_ids.length} rules in this suite.</p>
                        </div>
                      )}
                      <Button
                        className="w-full"
                        disabled={!selectedScannerUnitId || !selectedObservedDeviceId || !selectedSuiteId || running}
                        onClick={() => {
                          if (selectedScannerUnitId && selectedObservedDeviceId && selectedSuiteId) {
                            void runSuite(selectedScannerUnitId, selectedObservedDeviceId, selectedSuiteId);
                          }
                        }}
                      >
                        {running ? <Loader2 className="animate-spin" /> : null}
                        Run Suite
                      </Button>
                    </CardContent>
                  </Card>

                  <Card>
                    <CardHeader>
                      <CardTitle>Run Individual Test</CardTitle>
                      <CardDescription>
                        Execute one authored compliance rule directly against the selected DUT.
                      </CardDescription>
                    </CardHeader>
                    <CardContent className="space-y-3">
                      <Select value={selectedRuleId ?? undefined} onValueChange={setSelectedRuleId}>
                        <SelectTrigger className="w-full bg-background">
                          <SelectValue placeholder="Select rule" />
                        </SelectTrigger>
                        <SelectContent>
                          {rules.map((rule) => (
                            <SelectItem key={rule.id} value={rule.id}>
                              {(rule.title || rule.id) + " · " + rule.verdict}
                            </SelectItem>
                          ))}
                        </SelectContent>
                      </Select>
                      {selectedRule && (
                        <div className="rounded-md border bg-background p-3 text-xs text-muted-foreground">
                          <div className="flex items-center gap-2">
                            <p className="font-medium text-foreground">{selectedRule.title || selectedRule.id}</p>
                            <Badge variant={verdictVariant(selectedRule.verdict)}>{selectedRule.verdict}</Badge>
                          </div>
                          <p className="mt-1">{selectedRule.message}</p>
                          {selectedRule.reference ? <p className="mt-2 font-mono">{selectedRule.reference}</p> : null}
                        </div>
                      )}
                      <Button
                        className="w-full"
                        variant="outline"
                        disabled={!selectedScannerUnitId || !selectedObservedDeviceId || !selectedRuleId || running}
                        onClick={() => {
                          if (selectedScannerUnitId && selectedObservedDeviceId && selectedRuleId) {
                            void runRule(selectedScannerUnitId, selectedObservedDeviceId, selectedRuleId);
                          }
                        }}
                      >
                        {running ? <Loader2 className="animate-spin" /> : null}
                        Run Test
                      </Button>
                    </CardContent>
                  </Card>
                </div>

                <Card>
                  <CardHeader>
                    <CardTitle>Last Result</CardTitle>
                    <CardDescription>
                      {result
                        ? `Target ${result.target_id} for ${result.observed_device_name || result.observed_device_id} via ${result.scanner_unit_id}`
                        : "No compliance run has been executed in this session yet."}
                    </CardDescription>
                  </CardHeader>
                  <CardContent className="space-y-4">
                    {result ? (
                      <>
                        <div className="flex flex-wrap gap-2">
                          <Badge variant="outline">{result.rule_count} rules</Badge>
                          <Badge variant="outline" className="font-mono">{result.observed_device_id}</Badge>
                          <Badge variant="error">{counts.fail} fail</Badge>
                          <Badge variant="warn">{counts.warn} warn</Badge>
                          <Badge variant="info">{counts.info} info</Badge>
                        </div>
                        {result.findings.length === 0 ? (
                          <div className="rounded-md border bg-background p-4 text-sm text-muted-foreground">
                            PASS. No findings were raised for the selected DUT.
                          </div>
                        ) : (
                          <div className="space-y-3">
                            {result.findings.map((finding, index) => (
                              <div key={`${finding.rule_id}-${finding.observed_device_id}-${index}`} className="rounded-md border bg-background p-4">
                                <div className="flex flex-wrap items-center gap-2">
                                  <Badge variant={verdictVariant(finding.verdict)}>{finding.verdict}</Badge>
                                  <span className="text-sm font-medium">{finding.rule_id}</span>
                                  <span className="font-mono text-xs text-muted-foreground">
                                    {finding.observed_device_name || finding.observed_device_id}
                                  </span>
                                </div>
                                <p className="mt-2 text-sm text-foreground">{finding.message}</p>
                                {finding.reference ? (
                                  <p className="mt-2 font-mono text-xs text-muted-foreground">{finding.reference}</p>
                                ) : null}
                              </div>
                            ))}
                          </div>
                        )}
                      </>
                    ) : (
                      <div className="rounded-md border border-dashed bg-background p-4 text-sm text-muted-foreground">
                        Select a scanner unit, pick one detected DUT, and run either a suite or an individual test.
                      </div>
                    )}
                    {selectedScannerUnit && (
                      <p className="text-xs text-muted-foreground">
                        Scanner unit: <span className="font-mono">{selectedScannerUnit.id}</span>
                      </p>
                    )}
                  </CardContent>
                </Card>
              </>
            )}
          </div>
        </ScrollArea>
      </div>
    </>
  );
}
