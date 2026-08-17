// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/rust/weglet_ffi.h
//
// The C side of the boundary with rust/weglet-ffi.
//
// Hand-written rather than generated, and that is a liability: nothing
// checks it against the Rust signatures, so a mismatch here is undefined
// behaviour rather than a compile error. Two rules keep it manageable --
// no type crosses that is not a scalar or a C string, and every change
// here is made in the same commit as the Rust change it mirrors.
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

// Tabs.
size_t weglet_tab_count(const WegletState* state);
uint64_t weglet_tab_id_at(const WegletState* state, size_t index);
uint64_t weglet_active_tab_id(const WegletState* state);
char* weglet_tab_url(const WegletState* state, uint64_t id);
char* weglet_tab_label(const WegletState* state, uint64_t id);
bool weglet_tab_can_go_back(const WegletState* state, uint64_t id);
bool weglet_tab_can_go_forward(const WegletState* state, uint64_t id);

// Returns the new id, or 0 when the tab ceiling is reached.
uint64_t weglet_open_tab(WegletState* state, const char* url);
bool weglet_close_tab(WegletState* state, uint64_t id);
bool weglet_activate_tab(WegletState* state, uint64_t id);
void weglet_cycle_tab(WegletState* state, bool forward);
// One-based, as on the keyboard. 9 means the last tab.
bool weglet_activate_tab_at(WegletState* state, size_t position);
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
uint32_t weglet_assess_navigation(const char* url);
char* weglet_risk_title(const char* url);
char* weglet_risk_reason(const char* url);
bool weglet_is_host_blocked(const WegletState* state, const char* host);

// Settings and session.
bool weglet_terms_accepted(const WegletState* state);
void weglet_accept_terms(WegletState* state);
bool weglet_save_session(const WegletState* state);

}  // extern "C"

#endif  // WEGLET_RUST_WEGLET_FFI_H_
