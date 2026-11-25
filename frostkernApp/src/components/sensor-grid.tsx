import { StyleSheet } from "react-native";

import type { SensorId } from "@/constants/sensors";
import { useLiveData } from "@/context/live-data";
import SensorCard from "./sensor-card";
import { ThemedView } from "./themed-view";

// Grid order: inlets on the top row, outlets below.
const ORDER: SensorId[] = [
  "temperatureIn",
  "flowRateIn",
  "temperatureOut",
  "flowRateOut",
];

// Temperatures to 0.1 °C; flows to 0.01 L/min so a slow flow doesn't round to 0.
const DECIMALS: Record<SensorId, number> = {
  temperatureIn: 1,
  temperatureOut: 1,
  flowRateIn: 2,
  flowRateOut: 2,
};

export default function SensorGrid() {
  const { readings } = useLiveData();

  return (
    <ThemedView style={styles.grid}>
      {ORDER.map((sensor) => {
        const value = readings[sensor];
        return (
          <SensorCard
            key={sensor}
            sensor={sensor}
            // "—" = no valid reading (slave not reporting or sensor fault).
            value={value === null ? "—" : value.toFixed(DECIMALS[sensor])}
          />
        );
      })}
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  grid: {
    flexDirection: "row",
    flexWrap: "wrap",
    alignSelf: "stretch",
    gap: 12,
    padding: 10,
  },
});
