// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/modal.ts
//
// A modal that actually traps focus, and a context menu that closes.

import { byId } from "./dom.js";

interface ModalOptions {
  readonly backdropId: string;
  readonly onConfirm: () => void;
  // The field to put the cursor in when it opens.
  readonly focusId: string;
}

// Returns open and close. Both are needed by the caller, and neither belongs
// to the DOM node itself.
export function createModal(options: ModalOptions): {
  open: () => void;
  close: () => void;
} {
  const backdrop = byId(options.backdropId);

  const close = (): void => {
    backdrop.classList.remove("open");
  };

  const open = (): void => {
    backdrop.classList.add("open");
    byId<HTMLInputElement>(options.focusId).focus();
  };

  // Clicking the backdrop itself dismisses; clicking the modal inside it must
  // not, so the target has to be the backdrop and not a descendant.
  backdrop.addEventListener("click", (event) => {
    if (event.target === backdrop) {
      close();
    }
  });

  backdrop.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Escape") {
      close();
      return;
    }
    if (key === "Enter") {
      // Enter anywhere in the modal confirms, the way a real dialog does --
      // except in the Cancel button, where it would be a trap.
      if (document.activeElement?.id !== "shortcut-cancel") {
        event.preventDefault();
        options.onConfirm();
      }
      return;
    }
    if (key !== "Tab") {
      return;
    }

    // Focus trap. Without it Tab walks out of the dialog and into the page
    // behind it, which is invisible and unreachable -- the user is left
    // typing into nothing.
    const focusable = backdrop.querySelectorAll<HTMLElement>(
      "input, button, [tabindex]:not([tabindex='-1'])",
    );
    if (focusable.length === 0) {
      return;
    }
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    if (!first || !last) {
      return;
    }
    const keyboard = event as KeyboardEvent;
    if (keyboard.shiftKey && document.activeElement === first) {
      event.preventDefault();
      last.focus();
    } else if (!keyboard.shiftKey && document.activeElement === last) {
      event.preventDefault();
      first.focus();
    }
  });

  return { open, close };
}

interface MenuItem {
  readonly id: string;
  readonly onActivate: () => void;
}

// A context menu positioned at the pointer, clamped to the window.
export function createContextMenu(
  menuId: string,
  items: readonly MenuItem[],
): { openAt: (x: number, y: number) => void; close: () => void } {
  const menu = byId(menuId);

  const close = (): void => {
    menu.classList.remove("open");
  };

  const openAt = (x: number, y: number): void => {
    menu.classList.add("open");
    // Measured after it is displayed: a hidden element has no size, so the
    // clamp below would compare against zero.
    const rect = menu.getBoundingClientRect();
    const margin = 6;
    menu.style.left = `${Math.min(x, window.innerWidth - rect.width - margin)}px`;
    menu.style.top = `${Math.min(y, window.innerHeight - rect.height - margin)}px`;
  };

  for (const item of items) {
    byId(item.id).addEventListener("click", () => {
      close();
      item.onActivate();
    });
  }

  // Closed by anything that means "not this": a click elsewhere, Escape, or
  // the window losing focus. The previous interface only had the first, so a
  // menu opened by mistake stayed open.
  document.addEventListener("pointerdown", (event) => {
    if (!menu.contains(event.target as Node)) {
      close();
    }
  });
  document.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      close();
    }
  });
  window.addEventListener("blur", close);

  return { openAt, close };
}
