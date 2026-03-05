import { useCallback, useEffect, useState } from "react";
import { check, type Update } from "@tauri-apps/plugin-updater";
import { relaunch } from "@tauri-apps/plugin-process";

type UpdateStatus =
  | { state: "idle" }
  | { state: "checking" }
  | { state: "available"; update: Update }
  | { state: "downloading"; progress: number }
  | { state: "ready" }
  | { state: "error"; message: string };

export function useUpdater() {
  const [status, setStatus] = useState<UpdateStatus>({ state: "idle" });

  const checkForUpdate = useCallback(async () => {
    setStatus({ state: "checking" });
    try {
      const update = await check();
      if (update) {
        setStatus({ state: "available", update });
      } else {
        setStatus({ state: "idle" });
      }
    } catch (e) {
      setStatus({
        state: "error",
        message: e instanceof Error ? e.message : String(e),
      });
    }
  }, []);

  const downloadAndInstall = useCallback(async () => {
    if (status.state !== "available") return;
    const { update } = status;
    try {
      let totalLength = 0;
      let downloaded = 0;

      await update.downloadAndInstall((event) => {
        switch (event.event) {
          case "Started":
            totalLength = event.data.contentLength ?? 0;
            setStatus({ state: "downloading", progress: 0 });
            break;
          case "Progress":
            downloaded += event.data.chunkLength;
            setStatus({
              state: "downloading",
              progress: totalLength > 0 ? downloaded / totalLength : 0,
            });
            break;
          case "Finished":
            setStatus({ state: "ready" });
            break;
        }
      });

      setStatus({ state: "ready" });
    } catch (e) {
      setStatus({
        state: "error",
        message: e instanceof Error ? e.message : String(e),
      });
    }
  }, [status]);

  const restartApp = useCallback(async () => {
    await relaunch();
  }, []);

  // Check on mount (silently)
  useEffect(() => {
    checkForUpdate().catch(() => {
      // Silently ignore — no network or no releases yet
      setStatus({ state: "idle" });
    });
  }, [checkForUpdate]);

  return { status, checkForUpdate, downloadAndInstall, restartApp };
}
