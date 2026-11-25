// firebase/auth's package.json lists its web types before its React Native
// build, so TypeScript never sees getReactNativePersistence even though Metro
// bundles it on iOS/Android. Declare it here so firebase-auth.ts typechecks.
import type { Persistence, ReactNativeAsyncStorage } from "firebase/auth";

declare module "firebase/auth" {
  export function getReactNativePersistence(
    storage: ReactNativeAsyncStorage,
  ): Persistence;
}
