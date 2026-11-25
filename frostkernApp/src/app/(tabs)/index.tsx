import EngineStateCard from "@/components/engine-state-card";
import LiveStatus from "@/components/live-status";
import SensorGrid from "@/components/sensor-grid";
import { ThemedView } from "@/components/themed-view";
import { useSettings } from "@/context/settings";
import { StyleSheet } from "react-native";

export default function Index() {
  const { t } = useSettings();

  return (
    <ThemedView style={styles.container}>
      <EngineStateCard state="normal" detail={t("engineAllOk")} />
      <SensorGrid />
      <LiveStatus />
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
  },
});
