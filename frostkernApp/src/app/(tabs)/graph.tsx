import LineGraph from "@/components/line-graph";
import { ThemedView } from "@/components/themed-view";
import { Spacing } from "@/constants/theme";
import { Platform, ScrollView, StyleSheet } from "react-native";
import { useSafeAreaInsets } from "react-native-safe-area-context";

// Static for now. Later these will be the last 10 readings from the database.
const TEMPERATURE_IN = [
  28.1, 28.6, 29.0, 29.4, 29.2, 29.8, 30.3, 30.1, 29.9, 30.0,
];
const TEMPERATURE_OUT = [
  52.4, 54.1, 55.0, 56.8, 58.2, 57.5, 59.1, 60.4, 59.8, 60.0,
];
const FLOW_IN = [9.6, 9.8, 10.1, 10.0, 9.9, 10.3, 10.2, 10.0, 9.8, 10.0];
const FLOW_OUT = [8.8, 9.0, 9.1, 8.9, 9.2, 9.0, 8.7, 8.9, 9.1, 9.0];

export default function Index() {
  const insets = useSafeAreaInsets();

  return (
    <ThemedView style={styles.container}>
      <ScrollView
        contentContainerStyle={[
          styles.content,
          {
            paddingTop: insets.top + Spacing.three,
            // On iOS the tab bar floats over the content, and inside Native
            // Tabs insets.bottom already includes its height. Android already
            // keeps content above its tab bar.
            paddingBottom:
              (Platform.OS === "ios" ? insets.bottom : 0) + Spacing.three,
          },
        ]}
      >
        <LineGraph sensor="temperatureIn" data={TEMPERATURE_IN} />
        <LineGraph sensor="temperatureOut" data={TEMPERATURE_OUT} />
        <LineGraph sensor="flowRateIn" data={FLOW_IN} />
        <LineGraph sensor="flowRateOut" data={FLOW_OUT} />
      </ScrollView>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  content: {
    gap: 12,
  },
});
