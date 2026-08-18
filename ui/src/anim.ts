// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/anim.ts
//
// What makes the interface feel like it has weight.
//
// Nothing here is a CSS transition. A transition is a state flip with a
// duration on it: it starts from wherever the element was, runs for a fixed
// time and stops. Exponential relaxation instead means a control under the
// pointer is always heading towards where it should be at a rate
// proportional to how far off it is -- move the pointer away halfway
// through and it turns around from exactly where it got to, with no
// discontinuity and no queued animation to cancel.
//
// One requestAnimationFrame loop drives every registered strip and stops
// itself the moment everything has settled, so an idle window costs nothing.

import { tokens } from "./tokens.js";

// Time constant of the relaxation, in seconds. At this value a control
// reaches about 63% of the way in 55ms and is visually done in about 150ms
// -- fast enough to feel immediate, slow enough to read as movement.
const HOVER_TAU = tokens.motionHoverTau / 1000;

// How long a newly added element takes to arrive.
const ENTER_MS = tokens.motionEnter;

// Below this, a value is treated as having arrived. Without it the
// exponential never exactly reaches its target and the loop never stops.
const SETTLED = 0.002;

// Dock geometry, used only when a strip spreads.
const SLOT = tokens.layoutDockSlot;
const SIGMA_SLOTS = 1.6;

function approach(current: number, target: number, dt: number, tau: number): number {
  if (dt <= 0 || tau <= 0) {
    return target;
  }
  const gap = target - current;
  if (Math.abs(gap) <= SETTLED) {
    return target;
  }
  return current + gap * (1 - Math.exp(-dt / tau));
}

function isMoving(current: number, target: number): boolean {
  return Math.abs(target - current) > SETTLED;
}

function easeOutCubic(t: number): number {
  const clamped = Math.min(1, Math.max(0, t));
  const inverse = 1 - clamped;
  return 1 - inverse * inverse * inverse;
}

// Per-channel interpolation, because a colour halfway between two others is
// not either of them with an opacity.
export function mixChannel(from: number, to: number, t: number): number {
  return Math.round(from + (to - from) * Math.min(1, Math.max(0, t)));
}

export function mixRgb(
  from: readonly [number, number, number],
  to: readonly [number, number, number],
  t: number,
): [number, number, number] {
  return [
    mixChannel(from[0], to[0], t),
    mixChannel(from[1], to[1], t),
    mixChannel(from[2], to[2], t),
  ];
}

export interface Presentation {
  // 0 when cold, 1 when the pointer is on it. Everything visual is a
  // function of this rather than of a class being present.
  readonly heat: number;
  // 0 to 1 over ENTER_MS after the element first appeared.
  readonly arrival: number;
  readonly scale: number;
}

// A row of controls where the one under the pointer heats up.
//
// With `spreads`, its neighbours warm too, falling off as a gaussian -- that
// is the dock, where hovering one tile lifts the ones beside it.
export class Strip {
  private hovered: number | null = null;
  private heat: number[] = [];
  private born: number[] = [];
  private lastFrame = performance.now();

  constructor(
    private readonly maxScale: number,
    private readonly spreads: boolean,
  ) {}

  hover(index: number | null): void {
    this.hovered = index;
  }

  advance(count: number, now: number): boolean {
    this.resize(count, now);
    const dt = (now - this.lastFrame) / 1000;
    this.lastFrame = now;

    let moving = false;
    for (let i = 0; i < this.heat.length; i++) {
      const target = this.targetHeat(i);
      const current = this.heat[i] ?? 0;
      if (isMoving(current, target)) {
        moving = true;
      }
      this.heat[i] = approach(current, target, dt, HOVER_TAU);
    }

    // A strip is also moving while anything in it is still arriving, even
    // with the pointer nowhere near.
    for (const born of this.born) {
      if (now - born < ENTER_MS) {
        moving = true;
        break;
      }
    }
    return moving;
  }

  presentation(index: number, now: number): Presentation {
    const heat = this.heat[index] ?? 0;
    const born = this.born[index];
    const arrival = born === undefined ? 1 : easeOutCubic((now - born) / ENTER_MS);
    return { heat, arrival, scale: 1 + (this.maxScale - 1) * heat };
  }

  // Grows and shrinks with the number of controls. New slots are born now,
  // so they animate in; existing ones keep their heat, so rebuilding the tab
  // strip while the pointer rests on a tab does not reset it.
  private resize(count: number, now: number): void {
    while (this.heat.length < count) {
      this.heat.push(0);
      this.born.push(now);
    }
    this.heat.length = count;
    this.born.length = count;
    if (this.hovered !== null && this.hovered >= count) {
      this.hovered = null;
    }
  }

  private targetHeat(index: number): number {
    if (this.hovered === null) {
      return 0;
    }
    if (!this.spreads) {
      return index === this.hovered ? 1 : 0;
    }
    const offset = (index - this.hovered) * SLOT;
    const sigma = SLOT * SIGMA_SLOTS;
    // Past three sigma the contribution is under a thousandth; computing it
    // would just be arithmetic that rounds to the same pixel.
    if (Math.abs(offset) > sigma * 3) {
      return 0;
    }
    return Math.exp(-(offset * offset) / (2 * sigma * sigma));
  }
}

interface Registration {
  readonly strip: Strip;
  readonly count: () => number;
  readonly render: (now: number) => void;
}

const registrations: Registration[] = [];
let rafHandle: number | null = null;

export function register(
  strip: Strip,
  count: () => number,
  render: (now: number) => void,
): void {
  registrations.push({ strip, count, render });
}

// Asks for a frame if one is not already pending. Called on every pointer
// event; the loop stops on its own once nothing is moving, so there is no
// idle cost and nothing to remember to cancel.
export function scheduleFrame(): void {
  if (rafHandle === null) {
    rafHandle = requestAnimationFrame(tick);
  }
}

function tick(now: number): void {
  rafHandle = null;
  let anyMoving = false;
  for (const registration of registrations) {
    if (registration.strip.advance(registration.count(), now)) {
      anyMoving = true;
    }
    registration.render(now);
  }
  if (anyMoving) {
    scheduleFrame();
  }
}

// A ripple expanding from where the pointer actually landed, not from the
// button's centre.
//
// The caller's CSS has to give the element `position: relative` and
// `overflow: hidden`; this only creates and cleans up the element.
export function spawnRipple(el: HTMLElement, clientX: number, clientY: number): void {
  const rect = el.getBoundingClientRect();
  const x = clientX - rect.left;
  const y = clientY - rect.top;
  // Reaches the far corner whichever corner that is, so the ripple always
  // covers the whole button however off-centre the click was.
  const radius = Math.hypot(
    Math.max(x, rect.width - x),
    Math.max(y, rect.height - y),
  );

  const ripple = document.createElement("span");
  ripple.className = "ripple";
  ripple.style.left = `${x}px`;
  ripple.style.top = `${y}px`;
  ripple.style.width = `${radius * 2}px`;
  ripple.style.height = `${radius * 2}px`;
  el.appendChild(ripple);
  ripple.addEventListener("animationend", () => ripple.remove());
}

// One shared tooltip, moved to whichever control is hovered.
//
// No delay before it appears: a browser's own chrome shows tooltips
// essentially instantly, and the OS's title="" delay feels broken next to
// that. It is also why title="" is not used at all here.
let tooltipEl: HTMLElement | null = null;

function tooltip(): HTMLElement {
  if (tooltipEl !== null) {
    return tooltipEl;
  }
  const el = document.createElement("div");
  el.className = "tooltip";
  document.body.appendChild(el);
  tooltipEl = el;
  return el;
}

export function attachTooltip(el: HTMLElement, text: string): void {
  el.addEventListener("pointerenter", () => {
    const tip = tooltip();
    tip.textContent = text;
    const rect = el.getBoundingClientRect();

    const margin = 6;
    const width = tip.offsetWidth;
    const centre = rect.left + rect.width / 2 - width / 2;
    tip.style.left = `${Math.min(Math.max(centre, margin), window.innerWidth - width - margin)}px`;

    // A window cannot paint past its own edge, and the toolbar is a short
    // one -- so near the bottom the tooltip goes above the control instead.
    if (rect.bottom + 32 <= window.innerHeight) {
      tip.style.top = `${rect.bottom + 6}px`;
      tip.style.bottom = "";
    } else {
      tip.style.top = "";
      tip.style.bottom = `${window.innerHeight - rect.top + 6}px`;
    }
    tip.style.opacity = "1";
  });

  el.addEventListener("pointerleave", () => {
    if (tooltipEl !== null) {
      tooltipEl.style.opacity = "0";
    }
  });
}
