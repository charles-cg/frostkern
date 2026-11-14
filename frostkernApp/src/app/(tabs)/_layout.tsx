import { NativeTabs } from "expo-router/native-tabs";
import { Platform } from "react-native";

import { useSettings } from "@/context/settings";
import { useTheme } from "@/hooks/use-theme";

export default function TabLayout() {
  const { t } = useSettings();
  const theme = useTheme();

  // On Android the tab bar follows the phone's theme and only catches up with
  // the in-app light/dark choice after a tab change, so we give it our own
  // colors there. iOS draws its own glass tab bar and already follows along.
  const androidColors =
    Platform.OS === "android"
      ? {
          backgroundColor: theme.surface,
          iconColor: { default: theme.textSecondary, selected: theme.accent },
          labelStyle: {
            default: { color: theme.textSecondary },
            selected: { color: theme.accent },
          },
          // The pill behind the selected tab: the accent at ~15% opacity.
          indicatorColor: `${theme.accent}26`,
        }
      : {};

  return (
    <NativeTabs {...androidColors}>
      <NativeTabs.Trigger name="index">
        <NativeTabs.Trigger.Label>{t("tabDashboard")}</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="speedometer" md="speed" />
      </NativeTabs.Trigger>

      <NativeTabs.Trigger name="graph">
        <NativeTabs.Trigger.Label>{t("tabGraph")}</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="chart.xyaxis.line" md="show_chart" />
      </NativeTabs.Trigger>

      <NativeTabs.Trigger name="edit">
        <NativeTabs.Trigger.Label>{t("tabEdit")}</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="square.and.pencil" md="edit" />
      </NativeTabs.Trigger>

      <NativeTabs.Trigger name="more">
        <NativeTabs.Trigger.Label>{t("tabMore")}</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="ellipsis" md="more_horiz" />
      </NativeTabs.Trigger>
    </NativeTabs>
  );
}
