import type { FirebaseApp } from "firebase/app";
import { getAuth, type Auth } from "firebase/auth";

// Web. getAuth() already keeps the user in the browser's storage.
export function createAuth(app: FirebaseApp): Auth {
  return getAuth(app);
}
