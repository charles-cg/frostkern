import { StyleSheet } from "react-native";

import type { SensorId } from "@/constants/sensors";
import SensorCard from "./sensor-card";
import { ThemedView } from "./themed-view";

// Static for now. Later these will be the latest readings from the master.
// Order = grid order: inlets on the top row, outlets below.
const READINGS: { sensor: SensorId; value: string }[] = [
  { sensor: "temperatureIn", value: "30.0" },
  { sensor: "flowRateIn", value: "10.0" },
  { sensor: "temperatureOut", value: "60.0" },
  { sensor: "flowRateOut", value: "9.0" },
];

export default function SensorGrid() {
  return (
    <ThemedView style={styles.grid}>
      {READINGS.map(({ sensor, value }) => (
        <SensorCard key={sensor} sensor={sensor} value={value} />
      ))}
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
