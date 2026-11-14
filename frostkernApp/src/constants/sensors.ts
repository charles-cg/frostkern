import type { ThemeColor } from "@/constants/theme";

// Each sensor's id doubles as its translation key, so t(id) is its name.
export type SensorId =
  "temperatureIn" | "temperatureOut" | "flowRateIn" | "flowRateOut";

type SensorInfo = {
  unit: string;
  color: ThemeColor;
};

// One place that decides how each sensor looks, used by the dashboard
// cards and the graphs alike. Inlets share one color, outlets the other.
export const SENSORS: Record<SensorId, SensorInfo> = {
  temperatureIn: { unit: "°C", color: "sensorIn" },
  temperatureOut: { unit: "°C", color: "sensorOut" },
  flowRateIn: { unit: "L/min", color: "sensorIn" },
  flowRateOut: { unit: "L/min", color: "sensorOut" },
};
