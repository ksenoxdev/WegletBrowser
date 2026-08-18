// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/rust/weglet_ffi.h
//
// The C side of the boundary with rust/weglet-ffi.
//
// Hand-written, and every function here has to be checked against
// rust/weglet-ffi/src/lib.rs by hand -- nothing generates this file, so a
// mismatch is undefined behaviour rather than a compile error. This
// revision was written by listing every `extern "C" fn` in the Rust source
// and matching this file against that list one by one, because the two had
// gone out of step before: seven functions existed in Rust with nothing
// here to declare them.
//
// Two rules keep it manageable. No type crosses that is not a scalar or a
// C string. And every change here is made in the same commit as the Rust
// change it mirrors.
//
// Do not call any of this directly. WegletBridge wraps it so the raw
// pointers and manual frees stay in one file.

#ifndef WEGLET_RUST_WEGLET_FFI_H_
#define WEGLET_RUST_WEGLET_FFI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern "C" {

// Opaque. Created once per browser process, freed on shutdown.
typedef struct WegletState WegletState;

WegletState* weglet_state_new();
void weglet_state_free(WegletState* state);

// Every char* returned below is owned by the caller and must be released
// with weglet_string_free -- not free(), not delete: it came from Rust's
// allocator, which on Windows is a different heap.
void weglet_string_free(char* value);

// Windows.
//
// Every tab question below takes the window it is about. The model used to
// hold one flat tab list and one active tab, so a second window would have
// shown the same tabs and fought over which was in front -- while this side
// already counted windows and quit when the last one closed.
size_t weglet_window_count(const WegletState* state);
uint64_t weglet_window_id_at(const WegletState* state, size_t index);
// The new window's id, or 0 at the ceiling. Starts with one blank tab.
uint64_t weglet_open_window(WegletState* state);
// Its tabs go with it. Closing the last window leaves a fresh one.
bool weglet_close_window(WegletState* state, uint64_t window);
// Which window a tab is in. 0 is a real id, so a caller that has to tell
// "window 0" from "no such tab" asks about the tab first.
uint64_t weglet_tab_window(const WegletState* state, uint64_t id);

// Tabs. Scoped to one window; `id` is global once you have it.
size_t weglet_tab_count(const WegletState* state, uint64_t window);
uint64_t weglet_tab_id_at(const WegletState* state, uint64_t window, size_t index);
uint64_t weglet_active_tab_id(const WegletState* state, uint64_t window);
char* weglet_tab_url(const WegletState* state, uint64_t id);
char* weglet_tab_label(const WegletState* state, uint64_t id);
bool weglet_tab_can_go_back(const WegletState* state, uint64_t id);
bool weglet_tab_can_go_forward(const WegletState* state, uint64_t id);
// The model has kept this since weglet_tab_loading_changed existed;
// nothing could read it, so the tab strip carried a spinner with no way
// to know when to show it.
bool weglet_tab_loading(const WegletState* state, uint64_t id);

// Returns the new id, or 0 when the tab ceiling is reached.
uint64_t weglet_open_tab(WegletState* state, uint64_t window, const char* url);
bool weglet_close_tab(WegletState* state, uint64_t id);
bool weglet_activate_tab(WegletState* state, uint64_t id);
void weglet_cycle_tab(WegletState* state, uint64_t window, bool forward);
// One-based, as on the keyboard. 9 means the last tab.
bool weglet_activate_tab_at(WegletState* state, uint64_t window, size_t position);
bool weglet_reorder_tab(WegletState* state, uint64_t id, size_t target);

// Navigation. `navigated` adds a history entry; `url_replaced` is for a
// redirect or history.replaceState and does not, so Back still works.
void weglet_tab_navigated(WegletState* state, uint64_t id, const char* url);
void weglet_tab_url_replaced(WegletState* state, uint64_t id, const char* url);
char* weglet_tab_go_back(WegletState* state, uint64_t id);
char* weglet_tab_go_forward(WegletState* state, uint64_t id);
void weglet_tab_title_changed(WegletState* state, uint64_t id, const char* title);
void weglet_tab_loading_changed(WegletState* state, uint64_t id, bool loading);

// Omnibox: what the user typed, resolved to a URL to load.
char* weglet_omnibox_resolve(const WegletState* state, const char* input);

// Security. 0 = nothing to say, 1 = warn, 2 = block.
//
// One call, not four. `title`, `reason` and `host` may each be null when
// the caller does not want that part; a non-null one is always written,
// including on a 0 answer, where all three come back as empty strings.
// Every string written is owned by the caller.
//
// `host` is the host the risk is about, empty when there is none to show.
// The target URL is not returned -- the caller already has it.
//
// This is a pure function of the URL and takes no state. Whether the user
// blocked the site is a separate question: weglet_is_url_blocked.
uint32_t weglet_assess_navigation(const char* url,
                                  char** title,
                                  char** reason,
                                  char** host);

bool weglet_is_host_blocked(const WegletState* state, const char* host);
// The same question about a whole URL, with the wording for the notice.
//
// The host is pulled out on the Rust side on purpose -- a second host
// parser here would have to know that a backslash ends the authority and
// that the host follows the LAST '@', and getting that wrong means
// "google.com@evil.example" is checked as google.com. False for a URL
// with no host at all.
//
// `title` and `reason` may each be null; a non-null one is always written,
// empty when the answer is false. The wording lives with every other
// string the notice page shows, which is weglet-security -- the page
// renders what it is handed and has no words of its own.
bool weglet_is_url_blocked(const WegletState* state,
                           const char* url,
                           char** title,
                           char** reason);

// Shortcuts: the pinned sites on the new tab page. Ordered; addressed by
// position, the same way the profile stores them.
size_t weglet_shortcut_count(const WegletState* state);
char* weglet_shortcut_title(const WegletState* state, size_t index);
char* weglet_shortcut_url(const WegletState* state, size_t index);
// False when the dock is full.
bool weglet_add_shortcut(WegletState* state, const char* title, const char* url);
bool weglet_edit_shortcut(
    WegletState* state, size_t index, const char* title, const char* url);
bool weglet_remove_shortcut(WegletState* state, size_t index);

// The line shown under the new tab page's search field.
char* weglet_new_tab_hint(const WegletState* state);

// Settings: search. Engine ids are "duckduckgo" | "google" | "bing" |
// "custom"; an id the browser does not recognise leaves the setting
// unchanged rather than resetting it, which is what a settings page from a
// newer version sending an id this one does not know looks like.
char* weglet_search_engine(const WegletState* state);
bool weglet_set_search_engine(WegletState* state, const char* id);
// The built-in choices to list, excluding "custom" -- that one is not an
// entry on the list, it is what the id field and the template box together
// mean. Read weglet_engine_count and then _id_at / _label_at for 0..count.
size_t weglet_engine_count();
char* weglet_engine_id_at(size_t index);
char* weglet_engine_label_at(size_t index);
char* weglet_custom_search_url(const WegletState* state);
// Accepted even mid-edit; validated only when it is actually used, in
// weglet_omnibox_resolve.
void weglet_set_custom_search_url(WegletState* state, const char* url);

// Settings: general.
bool weglet_restore_session(const WegletState* state);
void weglet_set_restore_session(WegletState* state, bool on);

// Settings: appearance. Shape is "pill" | "rounded" | "square".
char* weglet_accent_color(const WegletState* state);
// False for anything that is not #RRGGBB; the colour is left unchanged.
bool weglet_set_accent_color(WegletState* state, const char* color);
char* weglet_address_bar_shape(const WegletState* state);
bool weglet_set_address_bar_shape(WegletState* state, const char* shape);

// Settings: blocked hosts. Ordered; addressed by position.
size_t weglet_blocked_host_count(const WegletState* state);
char* weglet_blocked_host_at(const WegletState* state, size_t index);
// Canonicalised before storing, so two spellings of the same host cannot
// silently disagree about whether it is blocked. False if already present.
bool weglet_block_host(WegletState* state, const char* host);
bool weglet_unblock_host(WegletState* state, size_t index);

// Settings and session.
bool weglet_terms_accepted(const WegletState* state);
void weglet_accept_terms(WegletState* state);
bool weglet_save_session(const WegletState* state);

// Changing a setting marks it, it does not write it: an atomic write ends
// in fsync, and doing that inside the click that flipped a toggle put the
// disk on the UI thread. The browser flushes on its own schedule instead
// -- see WegletBridge's timer -- and again at shutdown.
//
// weglet_flush_settings returns false when there was something to write
// and it could not be written; the change stays pending so the next flush
// retries. Calling it with nothing pending is a no-op that returns true,
// so a timer can call it unconditionally.
// How often to call the two above, in seconds. From the profile and
// clamped there, so a slow disk or a machine that gets closed abruptly
// can be tuned without a rebuild.
uint64_t weglet_settings_flush_seconds(const WegletState* state);
uint64_t weglet_session_save_seconds(const WegletState* state);

bool weglet_settings_dirty(const WegletState* state);
bool weglet_flush_settings(WegletState* state);

}  // extern "C"

#endif  // WEGLET_RUST_WEGLET_FFI_H_
