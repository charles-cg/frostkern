import { onAuthStateChanged, signInAnonymously } from "firebase/auth";
import { onValue, ref } from "firebase/database";
import {
  createContext,
  useContext,
  useEffect,
  useState,
  type ReactNode,
} from "react";

import type { SensorId } from "@/constants/sensors";
import { auth, db } from "@/lib/firebase";

// What the ESP32 bridge writes to /slaves/{IEEE}/telemetry.
// A missing tempC/flowLpm means that reading is not valid right now.
type Telemetry = {
  nodeId?: number;
  tempC?: number;
  flowLpm?: number;
  ts?: number;
};
type SlavesSnapshot = Record<string, { telemetry?: Telemetry }>;

// Which slave measures what: slave 1 sits on the inlet, slave 2 on the outlet.
const NODE_SENSORS: Record<number, { temp: SensorId; flow: SensorId }> = {
  1: { temp: "temperatureIn", flow: "flowRateIn" },
  2: { temp: "temperatureOut", flow: "flowRateOut" },
};

// The bridge writes /bridge/lastSeen every 30 s; three missed writes = offline.
const BRIDGE_TIMEOUT_MS = 90_000;

export type LiveStatus = "connecting" | "live" | "bridgeOffline" | "error";
export type Readings = Record<SensorId, number | null>;

type LiveDataValue = {
  status: LiveStatus;
  readings: Readings;
  // Server time (ms) of the newest telemetry, and how old it is now.
  updatedAt: number | null;
  ageMs: number | null;
  error: string | null;
};

const LiveDataContext = createContext<LiveDataValue | null>(null);

export function LiveDataProvider({ children }: { children: ReactNode }) {
  const [signedIn, setSignedIn] = useState(false);
  const [error, setError] = useState<string | null>(null);
  // undefined = not loaded yet, null = loaded but nothing there.
  const [slaves, setSlaves] = useState<SlavesSnapshot | null | undefined>();
  const [bridgeLastSeen, setBridgeLastSeen] = useState<number | null | undefined>();
  // Phone clock minus server clock; Firebase measures it for us.
  const [serverOffset, setServerOffset] = useState(0);
  // Ticks every second so "updated N s ago" counts up 1, 2, 3… instead of
  // jumping in 5 s steps, and the offline check stays current.
  const [now, setNow] = useState(() => Date.now());

  // Sign in once. The rules only let signed-in users read; an anonymous
  // account is enough and needs no screen.
  useEffect(() => {
    return onAuthStateChanged(auth, (user) => {
      if (user) {
        setSignedIn(true);
      } else {
        signInAnonymously(auth).catch((e) => setError(String(e.code ?? e)));
      }
    });
  }, []);

  // Live listeners: Firebase calls these on every change, no polling.
  useEffect(() => {
    if (!signedIn) return;
    const onError = (e: Error) => setError(e.message);
    const unsubscribers = [
      onValue(ref(db, "slaves"), (s) => setSlaves(s.val()), onError),
      onValue(ref(db, "bridge/lastSeen"), (s) => setBridgeLastSeen(s.val()), onError),
      onValue(ref(db, ".info/serverTimeOffset"), (s) => setServerOffset(s.val() ?? 0)),
    ];
    return () => unsubscribers.forEach((unsubscribe) => unsubscribe());
  }, [signedIn]);

  useEffect(() => {
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, []);

  const readings: Readings = {
    temperatureIn: null,
    temperatureOut: null,
    flowRateIn: null,
    flowRateOut: null,
  };
  let updatedAt: number | null = null;
  for (const slave of Object.values(slaves ?? {})) {
    const telemetry = slave.telemetry;
    const sensors = telemetry?.nodeId ? NODE_SENSORS[telemetry.nodeId] : undefined;
    if (!telemetry || !sensors) continue;
    readings[sensors.temp] = typeof telemetry.tempC === "number" ? telemetry.tempC : null;
    readings[sensors.flow] = typeof telemetry.flowLpm === "number" ? telemetry.flowLpm : null;
    if (typeof telemetry.ts === "number") {
      updatedAt = Math.max(updatedAt ?? 0, telemetry.ts);
    }
  }

  const serverNow = now + serverOffset;
  let status: LiveStatus;
  if (error) {
    status = "error";
  } else if (!signedIn || slaves === undefined || bridgeLastSeen === undefined) {
    status = "connecting";
  } else if (bridgeLastSeen === null || serverNow - bridgeLastSeen > BRIDGE_TIMEOUT_MS) {
    status = "bridgeOffline";
  } else {
    status = "live";
  }

  return (
    <LiveDataContext.Provider
      value={{
        status,
        readings,
        updatedAt,
        ageMs: updatedAt === null ? null : Math.max(0, serverNow - updatedAt),
        error,
      }}
    >
      {children}
    </LiveDataContext.Provider>
  );
}

export function useLiveData() {
  const context = useContext(LiveDataContext);
  if (!context) {
    throw new Error("useLiveData must be used inside <LiveDataProvider>");
  }
  return context;
}
