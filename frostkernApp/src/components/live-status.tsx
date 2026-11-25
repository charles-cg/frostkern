import { ThemedText } from "@/components/themed-text";
import type { ThemeColor } from "@/constants/theme";
import { useLiveData, type LiveStatus as Status } from "@/context/live-data";
import { useSettings } from "@/context/settings";
import type { TranslationKey } from "@/i18n/translations";

const STATUS_CONFIG: Record<Status, { label: TranslationKey; color: ThemeColor }> = {
  connecting: { label: "liveConnecting", color: "textSecondary" },
  live: { label: "liveOk", color: "success" },
  bridgeOffline: { label: "liveBridgeOffline", color: "warning" },
  error: { label: "liveError", color: "danger" },
};

// One line under the sensors: is the data live, and how old is it?
export default function LiveStatus() {
  const { status, ageMs, error } = useLiveData();
  const { t } = useSettings();
  const { label, color } = STATUS_CONFIG[status];

  let text = t(label);
  if (status === "live" && ageMs !== null) {
    text += " · " + t("liveUpdated").replace("{seconds}", String(Math.round(ageMs / 1000)));
  }
  if (status === "error" && error) {
    text += ` (${error})`;
  }

  return (
    <ThemedText type="small" themeColor={color}>
      {text}
    </ThemedText>
  );
}
