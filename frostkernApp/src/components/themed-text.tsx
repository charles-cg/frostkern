import { StyleSheet, Text, type TextProps } from "react-native";

import { ThemeColor } from "@/constants/theme";
import { useTheme } from "@/hooks/use-theme";

export type ThemedTextProps = TextProps & {
  type?: "default" | "title" | "subtitle" | "small" | "caption" | "value";
  themeColor?: ThemeColor;
};

export function ThemedText({
  style,
  type = "default",
  themeColor,
  ...rest
}: ThemedTextProps) {
  const theme = useTheme();

  return (
    <Text
      style={[{ color: theme[themeColor ?? "text"] }, styles[type], style]}
      {...rest}
    />
  );
}

const styles = StyleSheet.create({
  title: {
    fontSize: 30,
    lineHeight: 36,
    fontWeight: "700",
  },
  subtitle: {
    fontSize: 20,
    lineHeight: 26,
    fontWeight: "600",
  },
  default: {
    fontSize: 16,
    lineHeight: 24,
    fontWeight: "400",
  },
  small: {
    fontSize: 14,
    lineHeight: 20,
    fontWeight: "500",
  },
  caption: {
    fontSize: 12,
    lineHeight: 16,
    fontWeight: "400",
  },
  value: {
    fontSize: 40,
    lineHeight: 44,
    fontWeight: "600",
    fontVariant: ["tabular-nums"],
  },
});
