// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// key -> string, in whichever language the profile is set to. The
// tables themselves are generated -- see generate_i18n.py -- from
// ui/i18n/*.txt, one file per language.

import { LANGUAGES, TRANSLATIONS, type Language } from "./generated_i18n.js";

export { LANGUAGES };
export type { Language };

// Falls back to English, then to the bare key, so a gap in a
// translation -- or a key typo -- never renders as blank text.
export function t(key: string, lang: Language): string {
  return TRANSLATIONS[lang]?.[key] ?? TRANSLATIONS.en[key] ?? key;
}

// Fills in every element opted in via data-i18n (textContent),
// data-i18n-placeholder, or data-i18n-title -- for static markup, where
// t() at the point text is built in TypeScript doesn't apply.
export function applyI18n(lang: Language, root: ParentNode = document): void {
  for (const el of root.querySelectorAll<HTMLElement>("[data-i18n]")) {
    const key = el.dataset.i18n;
    if (key) {
      el.textContent = t(key, lang);
    }
  }
  for (const el of root.querySelectorAll<HTMLInputElement>("[data-i18n-placeholder]")) {
    const key = el.dataset.i18nPlaceholder;
    if (key) {
      el.placeholder = t(key, lang);
    }
  }
  for (const el of root.querySelectorAll<HTMLElement>("[data-i18n-title]")) {
    const key = el.dataset.i18nTitle;
    if (key) {
      el.title = t(key, lang);
    }
  }
}

export function isLanguage(value: unknown): value is Language {
  return typeof value === "string" && (LANGUAGES as readonly string[]).includes(value);
}
