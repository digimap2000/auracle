import { useCallback, useEffect, useState } from "react";
import {
  type ComplianceRuleInfo,
  type ComplianceRunResult,
  type ComplianceSuiteInfo,
  listComplianceRules,
  listComplianceSuites,
  runComplianceRule,
  runComplianceSuite,
} from "@/lib/tauri";

interface ComplianceState {
  rules: ComplianceRuleInfo[];
  suites: ComplianceSuiteInfo[];
  result: ComplianceRunResult | null;
  loading: boolean;
  running: boolean;
  error: string | null;
}

export function useCompliance() {
  const [state, setState] = useState<ComplianceState>({
    rules: [],
    suites: [],
    result: null,
    loading: true,
    running: false,
    error: null,
  });

  const refresh = useCallback(async () => {
    setState((prev) => ({ ...prev, loading: true, error: null }));
    try {
      const [rules, suites] = await Promise.all([
        listComplianceRules(),
        listComplianceSuites(),
      ]);
      setState((prev) => ({
        ...prev,
        rules,
        suites,
        loading: false,
        error: null,
      }));
    } catch (err) {
      setState((prev) => ({
        ...prev,
        loading: false,
        error: err instanceof Error ? err.message : String(err),
      }));
    }
  }, []);

  useEffect(() => {
    refresh();
  }, [refresh]);

  const runRule = useCallback(async (unitId: string, ruleId: string) => {
    setState((prev) => ({ ...prev, running: true, error: null }));
    try {
      const result = await runComplianceRule(unitId, ruleId);
      setState((prev) => ({ ...prev, result, running: false, error: null }));
      return result;
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      setState((prev) => ({ ...prev, running: false, error: message }));
      throw err;
    }
  }, []);

  const runSuite = useCallback(async (unitId: string, suiteId: string) => {
    setState((prev) => ({ ...prev, running: true, error: null }));
    try {
      const result = await runComplianceSuite(unitId, suiteId);
      setState((prev) => ({ ...prev, result, running: false, error: null }));
      return result;
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      setState((prev) => ({ ...prev, running: false, error: message }));
      throw err;
    }
  }, []);

  return {
    ...state,
    refresh,
    runRule,
    runSuite,
  };
}
