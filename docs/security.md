# Weglet's security model

Three files pointed at this document before it existed:
`rust/weglet-security/src/risk.rs`, `ui/src/newtab.ts` and
`ui/src/toolbar.ts`. Each of them defers an argument to it, so the
argument is here.

## What Weglet promises

**No request the user did not ask for.** Opening the browser, opening a
tab, opening a settings page: none of these talk to the network. There is
no telemetry, no update ping, no safe-browsing lookup, no favicon fetch,
no font fetch, no metrics upload.

This is a constraint on the code, not a preference, and it is what several
otherwise-odd decisions are for:

- **Fonts are files in the binary**, not a CDN request. `ui/fonts/` is
  embedded by `ui/build_ui.py` and referenced by `@font-face` rules
  generated into `tokens.css`.
- **Icons are path data in `ui/src/icons.ts`**, not fetched SVGs.
- **The new tab page draws a letter, not a favicon.** Asking a third party
  for a site's icon tells that party what the user has pinned. The tab
  strip does the same: only Weglet's own pages get a mark.
- **`WegletBrowserContext` returns `nullptr`** for the push messaging
  service, the platform notification service and the background sync
  controller. Each of those is a subsystem that can open a connection on
  its own.
- **`chrome://weglet/` has `default-src 'none'`.** A page of ours has no
  CSP source that reaches off the machine at all, so a future mistake
  fails loudly instead of quietly phoning home.

## The privilege boundary

Weglet's own pages are ordinary web pages that happen to have a channel to
the browser process. What separates them from the web is content's WebUI
bindings, and those are granted per frame, by
`WegletWebUIControllerFactory::UseWebUIForURL`.

That function is the boundary. It tests an exact scheme and an exact host:

```cpp
url.SchemeIs(content::kChromeUIScheme) && url.host() == kHost
```

Not a prefix test. A prefix test would accept
`chrome://weglet.evil.example/` and hand it `chrome.send()`.

Everything downstream follows from that answer. There is no allow-list in
the message handler deciding which pages may send which messages, because
a page that is not ours never receives a handler at all.

**Arguments are still checked, every message, every time.** The pages are
our own code, but they run in a renderer process, and a compromised
renderer is precisely what this boundary exists for. The check is
generated from `ui/contract.json`: `WegletMessageHandler::Validate`
compares what arrived against the arity and argument types the contract
declares.

## What the risk heuristics are, and are not

`rust/weglet-security/src/risk.rs` looks at an address and decides whether
it is worth saying something about: punycode, letters that are Latin in
disguise, mixed alphabets, brand names in domains that are not the
brand's, known shorteners, credential pages on throwaway TLDs.

**These are advisory, and they are wrong in both directions.** They miss
real phishing and they flag innocent sites. Nothing else in the browser
depends on them being right, and that is deliberate — a miss is a quality
bug, not a hole.

What keeps them honest is `tests/corpus.rs`, which measures the false
positive and false negative rate against `tests/corpus.txt` and fails when
either goes past its budget. Change a rule and that test says what the
change cost, in a number rather than an opinion.

Two levels:

- **Warning** — dismissible. Something is unusual, the user decides.
- **Block** — not dismissible. The lookalike is unambiguous, or the
  address is structurally unsafe (control characters, a backslash in the
  authority, credentials before the host).

The security notice page is styled and wired but is not yet shown by
anything: a blocked navigation is currently dropped with a log line. Until
the notice appears, `ProceedPastSecurityNotice` deliberately does not
proceed — resuming a navigation nobody was warned about is worse than
doing nothing.

## Where blocking happens, and where it does not

Every path the browser process controls asks `WegletWindow::ShouldBlock`
before handing a URL to the engine:

- what the user typed in the address bar,
- a link or `window.open`, through `OpenURLFromTab`,
- the URL a restored tab is opened at, which came off disk and may have
  been blocked since the session was written.

`ShouldBlock` checks the user's own block list and the built-in one first,
then the heuristics, and stops only on a hard block.

**Redirects go past all of it.** A page that loads and then redirects is
not something a window sees; catching that needs a throttle, which is the
content layer's own extension point (`CreateURLLoaderThrottles` on
`WegletContentBrowserClient`). Until that exists, the built-in
ad-and-tracker list in `blocklist.rs` blocks only at navigation time, not
per resource.

## The lists are data, and a profile can replace them

The blocklist, the brand rules and the sensitive-word list were const
arrays in Rust, which meant updating any of them was a recompile. They are
data files now — `rust/weglet-security/data/` and
`rust/weglet-profile/data/` — compiled in with `include_str!`.

Compiled in, not fetched: Weglet makes no request the user did not ask
for, so there is nothing to update them *from*, and a data file shipped
next to the binary is one more thing to sign and lose.

A profile can override any of them without a build. Drop one of these into
the profile directory and it replaces the built-in table entirely:

| File | Replaces |
|---|---|
| `blocklist.txt` | ad and tracker hosts |
| `brands.toml` | the brands the heuristics know |
| `sensitive_words.txt` | words that suggest a credential page |
| `engines.toml` | the search engines settings offers |

**Replaces, not extends.** Someone who supplies a list has a reason, and
silently keeping entries they removed would defeat it.

A malformed file is reported and ignored, never fatal, and the structured
ones are rejected whole rather than partly applied — half a brand table is
a browser that catches some impostors and waves others through, with
nothing to say which.

Nothing is ever written to these files, so a profile that never had one
never gets one.

## The host is parsed in one place

`weglet_is_url_blocked` takes a whole URL and pulls the host out on the
Rust side, using `weglet-url`. This is not a convenience. A second host
parser on the C++ side would have to know two things that are easy to get
wrong:

- a backslash ends the authority exactly like a slash, so
  `https://example.com\@evil.example` is host `example.com`;
- the host is what follows the **last** `@`, so
  `https://google.com@evil.example/` is `evil.example`.

Getting either wrong on the block-list path means the check runs against a
host the browser is not connected to.

## No `innerHTML`, anywhere

`ui/src/dom.ts` has no way to set markup from a string. Every string that
reaches the DOM goes through `textContent`, so a page title or a bookmark
name containing markup is text. Icons are built with `createElementNS` and
path data rather than parsed from SVG source, so there is no innerHTML
call anywhere for the next one to look normal next to.

`ui/src/trusted_types.d.ts` declares `TrustedHTML` and friends as opaque:
nothing in this codebase can construct one, because nothing needs to.

## What is deliberately not defended against

- **A user who has lost their machine.** There is no encryption at rest
  beyond what the platform provides. `settings.toml` and `session.toml`
  are plain files.
- **A malicious extension.** There is no extension system.
- **Network-level observation.** Weglet is not a VPN and does not claim to
  be. The block list is a local list; a blocked host still resolves.
- **A compromised Chromium.** Weglet is an embedder. Everything below the
  content layer's public interface is Chromium's sandbox and Chromium's
  problem, and keeping it that way is why `weglet/DEPS` allows only
  `content/public`.
