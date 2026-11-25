// Every piece of text in the app, per language.
// English is the reference: the other languages must have the same keys.
const en = {
  // Tabs
  tabDashboard: "Dashboard",
  tabGraph: "Graphs",
  tabEdit: "Edit",
  tabMore: "More",

  // Sensors
  temperatureIn: "Inlet temp",
  temperatureOut: "Outlet temp",
  flowRateIn: "Inlet flow",
  flowRateOut: "Outlet flow",

  // Engine state
  engineState: "Engine state",
  engineNormal: "Normal",
  engineWarning: "Warning",
  engineOverheat: "Overheat",
  engineAllOk: "All temperatures within limits",

  // Live data
  liveConnecting: "Connecting…",
  liveOk: "Live",
  liveUpdated: "updated {seconds} s ago",
  liveBridgeOffline: "Bridge offline: showing last known values",
  liveError: "Can't reach the database",

  // Edit
  ac: "AC",
  on: "On",
  off: "Off",
  temperatureTarget: "Temperature target",
  pumpFlowRate: "Pump flow rate",

  // More
  appearance: "Appearance",
  themeSystem: "System",
  themeLight: "Light",
  themeDark: "Dark",
  language: "Language",
};

export type TranslationKey = keyof typeof en;

const es: Record<TranslationKey, string> = {
  tabDashboard: "Panel",
  tabGraph: "Gráficas",
  tabEdit: "Editar",
  tabMore: "Más",

  temperatureIn: "Temp. entrada",
  temperatureOut: "Temp. salida",
  flowRateIn: "Flujo entrada",
  flowRateOut: "Flujo salida",

  engineState: "Estado del motor",
  engineNormal: "Normal",
  engineWarning: "Advertencia",
  engineOverheat: "Sobrecalentado",
  engineAllOk: "Todas las temperaturas dentro del límite",

  liveConnecting: "Conectando…",
  liveOk: "En vivo",
  liveUpdated: "actualizado hace {seconds} s",
  liveBridgeOffline: "Puente desconectado: últimos valores conocidos",
  liveError: "No se puede conectar a la base de datos",

  ac: "A/C",
  on: "Encendido",
  off: "Apagado",
  temperatureTarget: "Temperatura objetivo",
  pumpFlowRate: "Caudal de la bomba",

  appearance: "Apariencia",
  themeSystem: "Sistema",
  themeLight: "Claro",
  themeDark: "Oscuro",
  language: "Idioma",
};

export const translations = { en, es };

export type Language = keyof typeof translations;

// Shown in the language picker, each in its own language.
export const LANGUAGE_NAMES: Record<Language, string> = {
  en: "English",
  es: "Español",
};
