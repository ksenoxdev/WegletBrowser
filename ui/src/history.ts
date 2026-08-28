// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/history.ts
//
// Two tabs: address-bar submissions, and downloads, both backed by the
// profile.

import { spawnRipple } from "./anim.js";
import { byId, el, runPage } from "./dom.js";
import { applyI18n, t, type Language } from "./i18n.js";
import { setIcon, type IconName } from "./icons.js";
import {
  onHistory,
  send,
  type DownloadEntry,
  type SearchHistoryEntry,
} from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

type PanelName = "search" | "downloads";
const PANELS: readonly PanelName[] = ["search", "downloads"];
// Read by showPanel, which runs from a click outside any push.
let currentLanguage: Language = "en";

function showPanel(name: PanelName): void {
  const index = PANELS.indexOf(name);
  if (index < 0) {
    return;
  }
  byId("switcher").style.setProperty("--count", String(PANELS.length));
  byId("switcher-pill").style.setProperty("--index", String(index));

  for (const panel of PANELS) {
    const tab = byId(`tab-${panel}`);
    const active = panel === name;
    tab.classList.toggle("active", active);
    tab.setAttribute("aria-selected", String(active));
  }
  for (const node of document.querySelectorAll<HTMLElement>("[role='tabpanel']")) {
    node.hidden = node.dataset.panel !== name;
  }
  updateClearButtonLabel();
}

function activePanel(): PanelName {
  return PANELS.find((panel) => byId(`tab-${panel}`).classList.contains("active")) ?? "search";
}

function updateClearButtonLabel(): void {
  byId("clear-active-tab").textContent = t(
    activePanel() === "downloads" ? "history.clearDownloads" : "history.clearSearchHistory",
    currentLanguage,
  );
}

function wireSwitcher(): void {
  PANELS.forEach((panel, index) => {
    const tab = byId(`tab-${panel}`);
    tab.addEventListener("click", () => showPanel(panel));
    tab.addEventListener("keydown", (event: Event) => {
      const key = (event as KeyboardEvent).key;
      if (key === "Enter" || key === " ") {
        event.preventDefault();
        showPanel(panel);
        return;
      }
      const delta = key === "ArrowRight" ? 1 : key === "ArrowLeft" ? -1 : 0;
      if (delta === 0) {
        return;
      }
      event.preventDefault();
      const next = PANELS[(index + delta + PANELS.length) % PANELS.length];
      if (next) {
        showPanel(next);
        byId(`tab-${next}`).focus();
      }
    });
  });
  showPanel("search");
}

// Buckets a newest-first list into day headings ("Today", "Yesterday", or a
// plain date) without a date-formatting library -- this is the only place
// that needs one at all.
function dayLabel(unixSeconds: number): string {
  const date = new Date(unixSeconds * 1000);
  const today = new Date();
  const yesterday = new Date(today);
  yesterday.setDate(yesterday.getDate() - 1);
  const sameDay = (a: Date, b: Date): boolean =>
    a.getFullYear() === b.getFullYear() &&
    a.getMonth() === b.getMonth() &&
    a.getDate() === b.getDate();
  if (sameDay(date, today)) return "Today";
  if (sameDay(date, yesterday)) return "Yesterday";
  return date.toLocaleDateString(undefined, { month: "long", day: "numeric" });
}

function relativeTime(unixSeconds: number): string {
  const deltaSeconds = Math.max(0, Math.floor(Date.now() / 1000) - unixSeconds);
  if (deltaSeconds < 60) return "just now";
  const minutes = Math.floor(deltaSeconds / 60);
  if (minutes < 60) return `${minutes}m ago`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours}h ago`;
  return `${Math.floor(hours / 24)}d ago`;
}

// Maps a file extension to one of icons.ts's names.
const EXTENSION_ICONS: Readonly<Record<string, IconName>> = {
  png: "photo",
  jpg: "photo",
  jpeg: "photo",
  gif: "photo",
  webp: "photo",
  svg: "photo",
  bmp: "photo",
  zip: "file-zip",
  rar: "file-zip",
  "7z": "file-zip",
  tar: "file-zip",
  gz: "file-zip",
  pdf: "file-type-pdf",
};

function iconNameFor(filename: string, failed: boolean): IconName {
  if (failed) return "alert-triangle";
  const ext = filename.includes(".") ? filename.split(".").pop()?.toLowerCase() : "";
  return (ext && EXTENSION_ICONS[ext]) || "file";
}

function renderGrouped<T>(
  container: HTMLElement,
  items: readonly T[],
  timeOf: (item: T) => number,
  buildRow: (item: T, index: number) => HTMLElement,
): void {
  let currentLabel: string | null = null;
  items.forEach((item, index) => {
    const label = dayLabel(timeOf(item));
    if (label !== currentLabel) {
      currentLabel = label;
      container.appendChild(el("div", { className: "day-heading", text: label }));
    }
    container.appendChild(buildRow(item, index));
  });
}

function searchRow(entry: SearchHistoryEntry): HTMLElement {
  const icon = el("div", { className: "row-icon" });
  setIcon(icon, "history", 14);

  const row = el("div", {
    className: "row",
    children: [
      icon,
      el("div", {
        className: "row-text",
        children: [
          el("div", { className: "row-title", text: entry.query }),
          el("div", { className: "row-sub", text: entry.url }),
        ],
      }),
      el("div", { className: "row-time", text: relativeTime(entry.visitedAt) }),
    ],
    // el()'s onClick, not a bare listener, so the row is keyboard-reachable.
    onClick: (event) => {
      send("navigate", entry.url);
      spawnRipple(row, event.clientX, event.clientY);
    },
  });
  return row;
}

function downloadRow(item: DownloadEntry, index: number): HTMLElement {
  const failed = item.status === "failed";
  const inProgress = item.status === "in-progress";
  // Only a completed download has a finished file to open or reveal.
  const openable = item.status === "complete";
  const icon = el("div", {
    className: failed ? "row-icon failed" : inProgress ? "row-icon loading" : "row-icon",
  });
  setIcon(icon, iconNameFor(item.filename, failed), 14);

  const reveal = el("button", { className: "row-reveal", title: "Show in folder" });
  setIcon(reveal, "folder", 14);
  reveal.addEventListener("click", (event) => {
    event.stopPropagation();
    send("revealDownload", index);
  });

  const row = el("div", {
    className: "row",
    children: [
      icon,
      el("div", {
        className: "row-text",
        children: [
          el("div", { className: "row-title", text: item.filename }),
          el("div", {
            className: "row-sub",
            text: failed ? item.errorMessage || "Failed" : item.sizeLabel,
          }),
        ],
      }),
      el("div", { className: "row-time", text: relativeTime(item.startedAt) }),
      ...(failed ? [] : [reveal]),
    ],
    // Only an openable row gets onClick at all -- via el(), that also
    // means no tabIndex, matching the cursor below. Key omitted rather
    // than set to undefined: exactOptionalPropertyTypes treats those
    // differently.
    ...(openable
      ? {
          onClick: (event: MouseEvent) => {
            send("openDownload", index);
            spawnRipple(row, event.clientX, event.clientY);
          },
        }
      : {}),
  });
  if (!openable) {
    row.style.cursor = "default";
  }
  return row;
}

function render(searchEntries: readonly SearchHistoryEntry[], downloads: readonly DownloadEntry[]): void {
  const searchList = byId("search-list");
  for (const child of [...searchList.children]) {
    child.remove();
  }
  if (searchEntries.length === 0) {
    searchList.appendChild(
      el("div", { className: "empty", text: t("history.noSearchHistory", currentLanguage) }),
    );
  } else {
    renderGrouped(searchList, searchEntries, (entry) => entry.visitedAt, searchRow);
  }

  const downloadsList = byId("downloads-list");
  for (const child of [...downloadsList.children]) {
    child.remove();
  }
  if (downloads.length === 0) {
    downloadsList.appendChild(
      el("div", { className: "empty", text: t("history.noDownloads", currentLanguage) }),
    );
  } else {
    renderGrouped(downloadsList, downloads, (item) => item.startedAt, downloadRow);
  }
}

function main(): void {
  byId("done").addEventListener("click", () => send("goBack"));
  wireSwitcher();

  byId("clear-active-tab").addEventListener("click", () => {
    send(activePanel() === "downloads" ? "clearDownloadHistory" : "clearSearchHistory");
  });

  onHistory((state) => {
    currentLanguage = state.language;
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
    updateClearButtonLabel();
    render(state.searchEntries, state.downloads);
  });
}

runPage(main);
