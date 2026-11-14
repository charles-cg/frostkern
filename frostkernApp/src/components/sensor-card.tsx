import { StyleSheet } from "react-native";

import { ThemedText } from "@/components/themed-text";
import { ThemedView } from "@/components/themed-view";
import { SENSORS, type SensorId } from "@/constants/sensors";
import { Spacing } from "@/constants/theme";
import { useSettings } from "@/context/settings";
import { useTheme } from "@/hooks/use-theme";

type SensorCardProps = {
  sensor: SensorId;
  value: string;
};

export default function SensorCard({ sensor, value }: SensorCardProps) {
  const theme = useTheme();
  const { t } = useSettings();
  const { unit, color } = SENSORS[sensor];

  return (
    <ThemedView
      type="surface"
      style={[styles.card, { borderLeftColor: theme[color] }]}
    >
      <ThemedText type="subtitle" themeColor="textSecondary">
        {t(sensor)}
      </ThemedText>
      <ThemedText type="value">
        {value}
        <ThemedText type="small" themeColor="textSecondary">
          {" "}
          {unit}
        </ThemedText>
      </ThemedText>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  card: {
    borderRadius: 12,
    padding: Spacing.three,
    gap: Spacing.two,
    flexGrow: 1,
    flexBasis: "40%",
    borderLeftWidth: 4,
    // Cards in a row get the same height; this pins the value to the bottom,
    // so values line up even if one label wraps onto two lines.
    justifyContent: "space-between",
  },
});
