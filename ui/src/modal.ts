// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// A modal that traps focus, and a context menu that closes.

import { byId } from "./dom.js";

interface ModalOptions {
  readonly backdropId: string;
  readonly onConfirm: () => void;
  // The element to put focus on when it opens: a field for a modal that
  // asks for input, a button for one that only asks a question.
  readonly focusId: string;
  // Enter confirms unless focus is on this one, where it would be a
  // trap -- see the keydown handler below.
  readonly cancelId: string;
}

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
    byId(options.focusId).focus();
  };

    // The backdrop dismisses; the modal inside it must not, so the target
    // has to be the backdrop and not a descendant.
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
      // Enter confirms, the way a real dialog does -- except in Cancel,
      // where it would be a trap.
      if (document.activeElement?.id !== options.cancelId) {
        event.preventDefault();
        options.onConfirm();
      }
      return;
    }
    if (key !== "Tab") {
      return;
    }

    // Focus trap. Without it Tab walks into the page behind the dialog,
    // which is invisible and unreachable.
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
    // Measured after it is displayed: a hidden element has no size.
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

  // Closed by anything that means "not this": a click elsewhere, Escape,
  // or the window losing focus.
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
