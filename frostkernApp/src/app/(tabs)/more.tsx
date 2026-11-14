import SettingPicker from "@/components/setting-picker";
import { ThemedView } from "@/components/themed-view";
import { Spacing } from "@/constants/theme";
import { useSettings, type ThemePreference } from "@/context/settings";
import { LANGUAGE_NAMES, type Language } from "@/i18n/translations";
import { Platform, ScrollView, StyleSheet } from "react-native";
import { useSafeAreaInsets } from "react-native-safe-area-context";

export default function Index() {
  const insets = useSafeAreaInsets();
  const { theme, setTheme, language, setLanguage, t } = useSettings();

  const themeOptions: { label: string; value: ThemePreference }[] = [
    { label: t("themeSystem"), value: "system" },
    { label: t("themeLight"), value: "light" },
    { label: t("themeDark"), value: "dark" },
  ];

  const languageOptions = (Object.keys(LANGUAGE_NAMES) as Language[]).map(
    (code) => ({ label: LANGUAGE_NAMES[code], value: code }),
  );

  return (
    <ThemedView style={styles.container}>
      <ScrollView
        contentContainerStyle={[
          styles.content,
          {
            paddingTop: insets.top + Spacing.three,
            paddingBottom:
              (Platform.OS === "ios" ? insets.bottom : 0) + Spacing.three,
          },
        ]}
      >
        <SettingPicker
          label={t("appearance")}
          options={themeOptions}
          value={theme}
          onValueChange={setTheme}
        />
        <SettingPicker
          label={t("language")}
          options={languageOptions}
          value={language}
          onValueChange={setLanguage}
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
