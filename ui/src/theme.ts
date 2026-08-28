// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Applies the profile's accent colour and address bar shape to this
// page's own document -- every page does this on every push (see
// WegletStateService::SendTo), since each is a separate WebContents
// whose tokens.css custom properties are fixed at build time.

import { channels } from "./tokens.js";

export type AddressBarShape = "pill" | "rounded" | "square";

type Channel = readonly [number, number, number];

function hexToChannel(hex: string): Channel | null {
  const match = /^#([0-9a-fA-F]{6})$/.exec(hex);
  if (!match) {
    return null;
  }
  const value = match[1] as string;
  return [
    parseInt(value.slice(0, 2), 16),
    parseInt(value.slice(2, 4), 16),
    parseInt(value.slice(4, 6), 16),
  ];
}

// Matches generate_tokens.py's mix(): a plain linear blend, close
// enough to CSS's color-mix(in srgb, ...) for a hover tint.
function mix(a: Channel, b: Channel, ratio: number): Channel {
  return [
    Math.round(a[0] * ratio + b[0] * (1 - ratio)),
    Math.round(a[1] * ratio + b[1] * (1 - ratio)),
    Math.round(a[2] * ratio + b[2] * (1 - ratio)),
  ];
}

// The quick luminance check every browser's "pick readable text for an
// arbitrary background" code boils down to -- not full contrast-ratio
// maths, just enough to choose one of the two colours the rest of the
// interface already uses for light and dark text.
function contrastingChannel(rgb: Channel): Channel {
  const luminance = 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2];
  return luminance > 150 ? channels.background : channels.text;
}

function channelToHex(rgb: Channel): string {
  return `#${rgb.map((value) => value.toString(16).padStart(2, "0")).join("")}`;
}

// Channel-triplet equivalents of the CSS custom properties applyAccent
// sets, for anim.ts's Strip: it interpolates a hover glow every frame
// and can't afford to read a custom property back out of the cascade
// that often. Reset to tokens.ts's static defaults when there's no
// custom accent.
export const liveChannels: {
  ring: Channel;
  accentSolid: Channel;
  onAccentSolid: Channel;
  surfaceHover: Channel;
  surfacePressed: Channel;
} = {
  ring: channels.ring,
  accentSolid: channels.accentSolid,
  onAccentSolid: channels.onAccentSolid,
  surfaceHover: channels.surfaceHover,
  surfacePressed: channels.surfacePressed,
};

export function applyAccent(hex: string): void {
  const root = document.documentElement.style;
  const rgb = hexToChannel(hex);
  if (!rgb) {
    // Malformed or not sent: fall back to tokens.css's own default
    // rather than paint something broken.
    root.removeProperty("--color-ring");
    root.removeProperty("--color-accent-solid");
    root.removeProperty("--color-on-accent-solid");
    liveChannels.ring = channels.ring;
    liveChannels.accentSolid = channels.accentSolid;
    liveChannels.onAccentSolid = channels.onAccentSolid;
    liveChannels.surfaceHover = channels.surfaceHover;
    liveChannels.surfacePressed = channels.surfacePressed;
    return;
  }
  const onAccent = contrastingChannel(rgb);
  root.setProperty("--color-ring", hex);
  root.setProperty("--color-accent-solid", hex);
  root.setProperty("--color-on-accent-solid", channelToHex(onAccent));
  liveChannels.ring = rgb;
  liveChannels.accentSolid = rgb;
  liveChannels.onAccentSolid = onAccent;
  liveChannels.surfaceHover = mix(rgb, channels.field, 0.25);
  liveChannels.surfacePressed = mix(rgb, channels.field, 0.4);
}

export function applyAddressBarShape(shape: AddressBarShape): void {
  document.documentElement.dataset.addressShape = shape;
}

export function isAddressBarShape(value: unknown): value is AddressBarShape {
  return value === "pill" || value === "rounded" || value === "square";
}
