import { getApp, getApps, initializeApp } from "firebase/app";
import { getDatabase } from "firebase/database";

import { createAuth } from "./firebase-auth";

// Values come from .env.local (copy .env.example). EXPO_PUBLIC_ variables are
// copied into the app when it is bundled, so none of them may be a secret: the
// web API key only names the project. The database rules protect the data.
const firebaseConfig = {
  apiKey: process.env.EXPO_PUBLIC_FIREBASE_API_KEY,
  authDomain: process.env.EXPO_PUBLIC_FIREBASE_AUTH_DOMAIN,
  databaseURL: process.env.EXPO_PUBLIC_FIREBASE_DATABASE_URL,
  projectId: process.env.EXPO_PUBLIC_FIREBASE_PROJECT_ID,
  appId: process.env.EXPO_PUBLIC_FIREBASE_APP_ID,
};

// Fast Refresh re-runs this file; reuse the app instead of creating a second one.
export const app = getApps().length > 0 ? getApp() : initializeApp(firebaseConfig);
export const auth = createAuth(app);
export const db = getDatabase(app);
