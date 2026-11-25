import Storage from "expo-sqlite/kv-store";
import type { FirebaseApp } from "firebase/app";
import {
  getAuth,
  getReactNativePersistence,
  initializeAuth,
  type Auth,
} from "firebase/auth";

// iOS/Android. Firebase keeps the signed-in user in the same key-value store
// as the settings, so the anonymous account survives an app restart instead
// of a new one being created every launch.
export function createAuth(app: FirebaseApp): Auth {
  try {
    return initializeAuth(app, {
      persistence: getReactNativePersistence(Storage),
    });
  } catch {
    // Already initialized (Fast Refresh re-ran this file).
    return getAuth(app);
  }
}
