import { Platform } from "react-native";

export const Colors = {
  light: {
    text: "#14222B",
    background: "#F4F8FA",
    surface: "#FFFFFF",
    textSecondary: "#5B6B76",
    accent: "#0B7A9E",
    success: "#1E8E5A",
    warning: "#D4501A",
    danger: "#A3154F",
    // Inlet readings use the accent; outlet readings a warm gold.
    sensorIn: "#0B7A9E",
    sensorOut: "#C99A1E",
  },
  dark: {
    text: "#E8F1F5",
    background: "#0D1519",
    surface: "#16222A",
    textSecondary: "#93A5B0",
    accent: "#6FD3F0",
    success: "#5BD69A",
    warning: "#FF8A3D",
    danger: "#FF5A90",
    sensorIn: "#6FD3F0",
    sensorOut: "#F2D06B",
  },
} as const;

export type ThemeColor = keyof typeof Colors.light & keyof typeof Colors.dark;

export const Fonts = Platform.select({
  ios: {
    /** iOS `UIFontDescriptorSystemDesignDefault` */
    sans: "system-ui",
    /** iOS `UIFontDescriptorSystemDesignSerif` */
    serif: "ui-serif",
    /** iOS `UIFontDescriptorSystemDesignRounded` */
    rounded: "ui-rounded",
    /** iOS `UIFontDescriptorSystemDesignMonospaced` */
    mono: "ui-monospace",
  },
  default: {
    sans: "normal",
    serif: "serif",
    rounded: "normal",
    mono: "monospace",
  },
  web: {
    sans: "var(--font-display)",
    serif: "var(--font-serif)",
    rounded: "var(--font-rounded)",
    mono: "var(--font-mono)",
  },
});

export const Spacing = {
  half: 2,
  one: 4,
  two: 8,
  three: 16,
  four: 24,
  five: 32,
  six: 64,
} as const;

export const BottomTabInset = Platform.select({ ios: 50, android: 80 }) ?? 0;
export const MaxContentWidth = 800;
