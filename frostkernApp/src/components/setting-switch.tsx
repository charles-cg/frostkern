import { Switch } from "@expo/ui";
import { StyleSheet } from "react-native";

import { ThemedHost } from "@/components/themed-host";
import { ThemedText } from "@/components/themed-text";
import { ThemedView } from "@/components/themed-view";
import { Spacing } from "@/constants/theme";

type SettingSwitchProps = {
  label: string;
  description?: string;
  value: boolean;
  onValueChange: (value: boolean) => void;
};

export default function SettingSwitch({
  label,
  description,
  value,
  onValueChange,
}: SettingSwitchProps) {
  return (
    <ThemedView type="surface" style={styles.card}>
      <ThemedView type="surface" style={styles.text}>
        <ThemedText type="default">{label}</ThemedText>
        {description ? (
          <ThemedText type="caption" themeColor="textSecondary">
            {description}
          </ThemedText>
        ) : null}
      </ThemedView>

      {/* Host is the bridge between React Native and the native UI. */}
      <ThemedHost matchContents>
        <Switch value={value} onValueChange={onValueChange} />
      </ThemedHost>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  card: {
    flexDirection: "row",
    alignItems: "center",
    gap: Spacing.three,
    borderRadius: 12,
    padding: Spacing.three,
    alignSelf: "stretch",
    marginHorizontal: 10,
  },
  text: {
    flex: 1,
  },
});
