import { Picker } from "@expo/ui";
import { Platform, StyleSheet } from "react-native";

import { ThemedHost } from "@/components/themed-host";
import { ThemedText } from "@/components/themed-text";
import { ThemedView } from "@/components/themed-view";
import { Spacing } from "@/constants/theme";

type SettingPickerProps<T extends string> = {
  label: string;
  options: { label: string; value: T }[];
  value: T;
  onValueChange: (value: T) => void;
};

export default function SettingPicker<T extends string>({
  label,
  options,
  value,
  onValueChange,
}: SettingPickerProps<T>) {
  return (
    <ThemedView type="surface" style={styles.card}>
      <ThemedText type="default" style={styles.label}>
        {label}
      </ThemedText>

      {/* appearance="menu": a native dropdown menu on iOS and Android.
          On Android it's a Material text field that wants ~280dp, which
          squeezes the label, so we cap its width there. */}
      <ThemedHost
        matchContents={Platform.OS === "android" ? { vertical: true } : true}
        style={Platform.OS === "android" ? styles.androidPicker : undefined}
      >
        <Picker
          appearance="menu"
          selectedValue={value}
          onValueChange={onValueChange}
        >
          {options.map((option) => (
            <Picker.Item
              key={option.value}
              label={option.label}
              value={option.value}
            />
          ))}
        </Picker>
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
  label: {
    flex: 1,
  },
  androidPicker: {
    width: 150,
  },
});
