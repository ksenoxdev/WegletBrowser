// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/dom.ts
//
// Building blocks for the pages.

// Creates an element, sets its text and classes, appends children.
//
// Deliberately has no way to set innerHTML. Every string that reaches the
// DOM through here goes through textContent, so a page title or a
// bookmark name containing markup is text and not an element -- and there
// is no convenient escape hatch to forget about.
export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  options: {
    className?: string;
    text?: string;
    title?: string;
    children?: (Node | null)[];
    onClick?: (event: MouseEvent) => void;
  } = {},
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  if (options.className) {
    node.className = options.className;
  }
  if (options.text !== undefined) {
    node.textContent = options.text;
  }
  if (options.title !== undefined) {
    node.title = options.title;
  }
  for (const child of options.children ?? []) {
    if (child) {
      node.appendChild(child);
    }
  }
  if (options.onClick) {
    const onClick = options.onClick;
    node.addEventListener("click", (event: Event) => {
      onClick(event as MouseEvent);
    });
    // Anything clickable has to be reachable from the keyboard. The
    // previous interface had one aria-label across five thousand lines,
    // and a tab strip made of divs that a screen reader could not see.
    if (!(node instanceof HTMLButtonElement) && !(node instanceof HTMLAnchorElement)) {
      node.tabIndex = 0;
      node.setAttribute("role", "button");
      // Enter and Space are what a real button responds to, so a div
      // standing in for one has to as well.
      node.addEventListener("keydown", (event: Event) => {
        const key = (event as KeyboardEvent).key;
        if (key === "Enter" || key === " ") {
          event.preventDefault();
          node.click();
        }
      });
    }
  }
  return node;
}

// Replaces an element's children in one go.
export function replaceChildren(parent: Element, children: Node[]): void {
  parent.replaceChildren(...children);
}

export function byId<T extends HTMLElement>(id: string): T {
  const node = document.getElementById(id);
  if (!node) {
    // A missing id means the markup and the script disagree, which is a
    // build-time mistake -- better loud than a page that half works.
    throw new Error(`weglet: no element with id "${id}"`);
  }
  return node as T;
}

// Runs a page's main() and reports a failure instead of letting it kill the
// whole module silently. byId throws on a missing element -- deliberately,
// so a mismatch between the markup and the script is loud during
// development -- but an uncaught exception in a module stops that page's
// entire script, not just the one control that was missing. One typo in an
// id should not mean the whole toolbar stops responding.
export function runPage(main: () => void): void {
  try {
    main();
  } catch (error) {
    console.error("weglet: page script failed to start", error);
  }
}
