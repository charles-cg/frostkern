import { TextInput } from "@expo/ui";
import { StyleSheet } from "react-native";

import { ThemedHost } from "@/components/themed-host";
import { ThemedText } from "@/components/themed-text";
import { ThemedView } from "@/components/themed-view";
import { Spacing } from "@/constants/theme";
import { useTheme } from "@/hooks/use-theme";

type SettingInputProps = {
  label: string;
  unit: string;
  defaultValue: string;
  onChangeText: (text: string) => void;
};

export default function SettingInput({
  label,
  unit,
  defaultValue,
  onChangeText,
}: SettingInputProps) {
  const theme = useTheme();

  return (
    <ThemedView type="surface" style={styles.card}>
      <ThemedText type="small" themeColor="textSecondary">
        {label}
      </ThemedText>

      <ThemedView type="surface" style={styles.row}>
        {/* matchContents vertical: the Host takes the input's height, while
            flex: 1 lets it fill the row's width. */}
        <ThemedHost matchContents={{ vertical: true }} style={styles.host}>
          <TextInput
            defaultValue={defaultValue}
            onChangeText={onChangeText}
            keyboardType="decimal-pad"
            placeholder="0.0"
            style={{
              backgroundColor: theme.background,
              borderRadius: 8,
              paddingHorizontal: Spacing.three,
              paddingVertical: Spacing.two,
            }}
            textStyle={{ fontSize: 20, fontWeight: "600", color: theme.text }}
          />
        </ThemedHost>
        <ThemedText
          type="default"
          themeColor="textSecondary"
          style={styles.unit}
        >
          {unit}
        </ThemedText>
      </ThemedView>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  card: {
    gap: Spacing.two,
    borderRadius: 12,
    padding: Spacing.three,
    alignSelf: "stretch",
    marginHorizontal: 10,
  },
  row: {
    flexDirection: "row",
    alignItems: "center",
    gap: Spacing.two,
  },
  host: {
    flex: 1,
  },
  // Fixed width so every input box lines up, whatever the unit's length.
  unit: {
    width: 48,
  },
});
