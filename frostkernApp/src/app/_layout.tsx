import { Stack } from "expo-router";

import { LiveDataProvider } from "@/context/live-data";
import { SettingsProvider } from "@/context/settings";

export default function RootLayout() {
  return (
    <SettingsProvider>
      {/* One Firebase connection shared by every screen. */}
      <LiveDataProvider>
        <Stack screenOptions={{ headerShown: false }}>
          <Stack.Screen name="(tabs)" />
        </Stack>
      </LiveDataProvider>
    </SettingsProvider>
  );
}
