import { StyleSheet } from "react-native";

import { ThemedText } from "@/components/themed-text";
import { ThemedView } from "@/components/themed-view";
import { Spacing, type ThemeColor } from "@/constants/theme";
import { useSettings } from "@/context/settings";
import { useTheme } from "@/hooks/use-theme";
import type { TranslationKey } from "@/i18n/translations";

export type EngineState = "normal" | "warning" | "overheat";

const STATE_CONFIG: Record<
  EngineState,
  { label: TranslationKey; color: ThemeColor }
> = {
  normal: { label: "engineNormal", color: "success" },
  warning: { label: "engineWarning", color: "warning" },
  overheat: { label: "engineOverheat", color: "danger" },
};

type EngineStateCardProps = {
  state: EngineState;
  detail?: string;
};

export default function EngineStateCard({
  state,
  detail,
}: EngineStateCardProps) {
  const theme = useTheme();
  const { t } = useSettings();
  const { label, color } = STATE_CONFIG[state];

  return (
    <ThemedView
      type="surface"
      style={[styles.card, { borderLeftColor: theme[color] }]}
    >
      <ThemedView style={[styles.dot, { backgroundColor: theme[color] }]} />
      <ThemedView type="surface" style={styles.content}>
        <ThemedText type="default" themeColor="textSecondary">
          {t("engineState")}
        </ThemedText>
        <ThemedText type="title" themeColor={color}>
          {t(label)}
        </ThemedText>
        {detail ? (
          <ThemedText type="small" themeColor="textSecondary">
            {detail}
          </ThemedText>
        ) : null}
      </ThemedView>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  card: {
    flexDirection: "row",
    alignItems: "center",
    gap: Spacing.three,
    borderRadius: 12,
    paddingHorizontal: Spacing.three,
    paddingVertical: Spacing.four,
    borderLeftWidth: 4,
    alignSelf: "stretch",
    marginHorizontal: 10,
  },
  dot: {
    width: 16,
    height: 16,
    borderRadius: 8,
  },
  content: {
    flex: 1,
  },
});
