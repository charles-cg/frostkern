import Storage from "expo-sqlite/kv-store";
import { createContext, useContext, useState, type ReactNode } from "react";
import { Appearance } from "react-native";

import {
  translations,
  type Language,
  type TranslationKey,
} from "@/i18n/translations";

export type ThemePreference = "system" | "light" | "dark";

const THEME_KEY = "settings.theme";
const LANGUAGE_KEY = "settings.language";

const THEMES: ThemePreference[] = ["system", "light", "dark"];

// Read the saved values. Anything missing or unknown (first launch, or a
// value from an older version) falls back to the default.
function loadTheme(): ThemePreference {
  const saved = Storage.getItemSync(THEME_KEY);
  return THEMES.includes(saved as ThemePreference)
    ? (saved as ThemePreference)
    : "system";
}

function loadLanguage(): Language {
  const saved = Storage.getItemSync(LANGUAGE_KEY);
  return saved !== null && saved in translations ? (saved as Language) : "en";
}

function applyTheme(theme: ThemePreference) {
  // Overrides the color scheme for the whole app, native parts included
  // (tab bar, Expo UI controls). "unspecified" goes back to the phone's.
  Appearance.setColorScheme(theme === "system" ? "unspecified" : theme);
}

// Runs once when the app starts, before the first screen is drawn, so a saved
// dark/light choice doesn't flash the phone's theme first.
applyTheme(loadTheme());

type SettingsContextValue = {
  theme: ThemePreference;
  setTheme: (theme: ThemePreference) => void;
  language: Language;
  setLanguage: (language: Language) => void;
  t: (key: TranslationKey) => string;
};

const SettingsContext = createContext<SettingsContextValue | null>(null);

export function SettingsProvider({ children }: { children: ReactNode }) {
  // The function form runs only on the first render, so storage is read once.
  const [theme, setThemeState] = useState(loadTheme);
  const [language, setLanguageState] = useState(loadLanguage);

  const setTheme = (next: ThemePreference) => {
    setThemeState(next);
    applyTheme(next);
    Storage.setItemSync(THEME_KEY, next);
  };

  const setLanguage = (next: Language) => {
    setLanguageState(next);
    Storage.setItemSync(LANGUAGE_KEY, next);
  };

  const t = (key: TranslationKey) => translations[language][key];

  return (
    <SettingsContext.Provider
      value={{ theme, setTheme, language, setLanguage, t }}
    >
      {children}
    </SettingsContext.Provider>
  );
}

export function useSettings() {
  const context = useContext(SettingsContext);
  if (!context) {
    throw new Error("useSettings must be used inside <SettingsProvider>");
  }
  return context;
}
