import SettingInput from "@/components/setting-input";
import SettingSwitch from "@/components/setting-switch";
import { ThemedView } from "@/components/themed-view";
import { Spacing } from "@/constants/theme";
import { useSettings } from "@/context/settings";
import { useState } from "react";
import { Platform, ScrollView, StyleSheet } from "react-native";
import { useSafeAreaInsets } from "react-native-safe-area-context";

export default function Index() {
  const insets = useSafeAreaInsets();
  const { t } = useSettings();

  // Frontend only for now. Later these will be sent to the master.
  const [acOn, setAcOn] = useState(true);
  const [tempTarget, setTempTarget] = useState("40.0");
  const [flowRate, setFlowRate] = useState("12.0");

  return (
    <ThemedView style={styles.container}>
      <ScrollView
        // Tapping outside an input or dragging the list closes the keyboard.
        keyboardShouldPersistTaps="handled"
        keyboardDismissMode="on-drag"
        contentContainerStyle={[
          styles.content,
          {
            paddingTop: insets.top + Spacing.three,
            paddingBottom:
              (Platform.OS === "ios" ? insets.bottom : 0) + Spacing.three,
          },
        ]}
      >
        <SettingSwitch
          label={t("ac")}
          description={acOn ? t("on") : t("off")}
          value={acOn}
          onValueChange={setAcOn}
        />
        <SettingInput
          label={t("temperatureTarget")}
          unit="°C"
          defaultValue={tempTarget}
          onChangeText={setTempTarget}
        />
        <SettingInput
          label={t("pumpFlowRate")}
          unit="L/min"
          defaultValue={flowRate}
          onChangeText={setFlowRate}
        />
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
