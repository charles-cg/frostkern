import { Host } from "@expo/ui";
import type { ComponentProps } from "react";

import { useColorScheme } from "@/hooks/use-color-scheme";

type ThemedHostProps = ComponentProps<typeof Host>;

// A Host that follows the app's theme instead of the phone's.
// Without colorScheme, Android's Compose controls (switch, picker, inputs)
// read the phone setting and ignore the in-app light/dark choice.
export function ThemedHost(props: ThemedHostProps) {
  const scheme = useColorScheme();

  return (
    <Host colorScheme={scheme === "dark" ? "dark" : "light"} {...props} />
  );
}
