// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-ffi/src/lib.rs
//
// The only place C++ and Rust touch.
//
// Rules for everything in this file:
//
//   * No Rust type crosses the boundary. C++ sees an opaque handle and
//     C strings, nothing else.
//   * Every string handed to C++ is owned by Rust and freed by Rust, via
//     weglet_string_free. C++ never frees a pointer from here, and never
//     keeps one past the next call that could change the state.
//   * Nothing panics across the boundary. panic = "abort" in the release
//     profile makes unwinding into C++ impossible, and every entry point
//     handles its own failure instead of relying on that.
//   * A null or unknown handle is a no-op with a defined return, not a
//     crash. C++ has bugs too.

mod string;

use std::ffi::c_char;

use weglet_core::{AppState, OmniboxAction, TabId};
use weglet_profile::{Paths, Session, SessionTab, Settings};

use crate::string::{into_c_string, str_from_c};

// Repeated on every entry point below rather than written once, because
// clippy wants it per function and because a caller reading one signature
// should not have to go looking for the rules.
macro_rules! safety_doc {
    () => {
        "# Safety\n\n\
         `state` must be null or a live pointer from [`weglet_state_new`] \
         that has not been passed to [`weglet_state_free`]. Every string \
         argument must be null or NUL-terminated and valid for the call. \
         All calls must come from the browser process's UI thread and only \
         that thread. Any returned string is owned by the caller and freed \
         with `weglet_string_free`."
    };
}

// Everything the browser knows, behind one pointer. C++ holds this for
// the life of the process and passes it back on every call.
pub struct WegletState {
    state: AppState,
    settings: Settings,
    paths: Option<Paths>,
}

// SAFETY: the caller must be the browser process's UI thread, and only
// that thread. Nothing here is synchronised, because the state it guards
// is single-threaded by design -- the same rule the C++ side already
// follows for WebContents.
#[no_mangle]
pub extern "C" fn weglet_state_new() -> *mut WegletState {
    let paths = Paths::discover().ok();

    // A settings file that will not parse must not stop the browser from
    // opening. The C++ side logs the error; here the fallback is defaults.
    let settings = paths
        .as_ref()
        .and_then(|paths| Settings::load(&paths.settings_file()).ok())
        .unwrap_or_default();

    // Never restore before the terms are accepted: the first tab has to be
    // the terms screen, not whatever was open last time.
    let state = if settings.restore_session && settings.terms_accepted {
        paths
            .as_ref()
            .and_then(|paths| Session::load(&paths.session_file()).ok())
            .map(|session| {
                let (tabs, active) = session.to_restore_input();
                AppState::restore(tabs, active)
            })
            .unwrap_or_default()
    } else {
        AppState::new()
    };

    Box::into_raw(Box::new(WegletState {
        state,
        settings,
        paths,
    }))
}

// SAFETY: `state` must come from weglet_state_new and must not be used
// afterwards. Passing null is allowed and does nothing.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_state_free(state: *mut WegletState) {
    if state.is_null() {
        return;
    }
    // SAFETY: checked non-null above, and the contract says this pointer
    // came from Box::into_raw in weglet_state_new.
    drop(unsafe { Box::from_raw(state) });
}

/// Turns a raw handle into a reference, or None. Every entry point starts
/// here so a null from C++ is a defined no-op rather than a crash.
///
/// # Safety
///
/// `state` must be null or a live pointer from `weglet_state_new`.
unsafe fn as_ref<'a>(state: *const WegletState) -> Option<&'a WegletState> {
    // SAFETY: null-checked; validity is the caller's contract.
    unsafe { state.as_ref() }
}

/// # Safety
///
/// As [`as_ref`], and no other reference to the state may be live.
unsafe fn as_mut<'a>(state: *mut WegletState) -> Option<&'a mut WegletState> {
    // SAFETY: null-checked; validity and exclusivity are the caller's
    // contract.
    unsafe { state.as_mut() }
}

// ---------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_count(state: *const WegletState) -> usize {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(0, |state| state.state.tabs().len())
}

// The id at `index`, or 0 if out of range. Ids start at 0 too, so callers
// check the count first -- which is why this pairs with weglet_tab_count.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_id_at(state: *const WegletState, index: usize) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.tabs().get(index))
        .map_or(0, |tab| tab.id.value())
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_active_tab_id(state: *const WegletState) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(0, |state| state.state.active_id().value())
}

// Owned by the caller. Free with weglet_string_free.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_url(state: *const WegletState, id: u64) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let url = unsafe { as_ref(state) }
        .and_then(|state| state.state.tab(TabId::new(id)))
        .map(|tab| tab.url().to_string())
        .unwrap_or_default();
    into_c_string(url)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_label(state: *const WegletState, id: u64) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let label = unsafe { as_ref(state) }
        .and_then(|state| state.state.tab(TabId::new(id)))
        .map(|tab| tab.label().to_string())
        .unwrap_or_default();
    into_c_string(label)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_can_go_back(state: *const WegletState, id: u64) -> bool {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.tab(TabId::new(id)))
        .is_some_and(|tab| tab.history.can_go_back())
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_can_go_forward(state: *const WegletState, id: u64) -> bool {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.tab(TabId::new(id)))
        .is_some_and(|tab| tab.history.can_go_forward())
}

// The new tab's id, or 0 when the tab ceiling is reached. `url` may be
// null, which opens a blank tab.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_open_tab(state: *mut WegletState, url: *const c_char) -> u64 {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return 0;
    };
    // SAFETY: contract of str_from_c.
    let url = unsafe { str_from_c(url) }.unwrap_or(weglet_core::BLANK_TAB);
    state.state.open_tab(url).map_or(0, |id| id.value())
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_close_tab(state: *mut WegletState, id: u64) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }.is_some_and(|state| state.state.close_tab(TabId::new(id)))
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_activate_tab(state: *mut WegletState, id: u64) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }.is_some_and(|state| state.state.activate(TabId::new(id)))
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_cycle_tab(state: *mut WegletState, forward: bool) {
    // SAFETY: contract of as_mut.
    if let Some(state) = unsafe { as_mut(state) } {
        state.state.cycle(forward);
    }
}

// One-based, as on the keyboard. 9 means the last tab.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_activate_tab_at(state: *mut WegletState, position: usize) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }.is_some_and(|state| state.state.activate_by_position(position))
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_reorder_tab(
    state: *mut WegletState,
    id: u64,
    target: usize,
) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }.is_some_and(|state| state.state.reorder(TabId::new(id), target))
}

// ---------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------

// A new navigation: adds a history entry and drops the forward branch.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_navigated(
    state: *mut WegletState,
    id: u64,
    url: *const c_char,
) {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return;
    };
    // SAFETY: contract of str_from_c.
    let Some(url) = (unsafe { str_from_c(url) }) else {
        return;
    };
    if let Some(tab) = state.state.tab_mut(TabId::new(id)) {
        tab.navigate(url.to_string());
    }
}

// A redirect, or history.replaceState: the same entry, a different URL.
// Separate from weglet_tab_navigated because adding an entry here would
// make Back land the user on the page that redirected them.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_url_replaced(
    state: *mut WegletState,
    id: u64,
    url: *const c_char,
) {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return;
    };
    // SAFETY: contract of str_from_c.
    let Some(url) = (unsafe { str_from_c(url) }) else {
        return;
    };
    if let Some(tab) = state.state.tab_mut(TabId::new(id)) {
        tab.history.replace_current(url.to_string());
    }
}

// The URL to load, or an empty string when there is nowhere to go. Owned
// by the caller.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_go_back(state: *mut WegletState, id: u64) -> *mut c_char {
    // SAFETY: contract of as_mut.
    let url = unsafe { as_mut(state) }
        .and_then(|state| state.state.tab_mut(TabId::new(id)))
        .and_then(|tab| tab.history.go_back())
        .unwrap_or_default()
        .to_string();
    into_c_string(url)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_go_forward(state: *mut WegletState, id: u64) -> *mut c_char {
    // SAFETY: contract of as_mut.
    let url = unsafe { as_mut(state) }
        .and_then(|state| state.state.tab_mut(TabId::new(id)))
        .and_then(|tab| tab.history.go_forward())
        .unwrap_or_default()
        .to_string();
    into_c_string(url)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_title_changed(
    state: *mut WegletState,
    id: u64,
    title: *const c_char,
) {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return;
    };
    // SAFETY: contract of str_from_c.
    let title = unsafe { str_from_c(title) }.unwrap_or_default();
    if let Some(tab) = state.state.tab_mut(TabId::new(id)) {
        tab.title = title.to_string();
    }
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_loading_changed(
    state: *mut WegletState,
    id: u64,
    loading: bool,
) {
    // SAFETY: contract of as_mut.
    if let Some(state) = unsafe { as_mut(state) } {
        if let Some(tab) = state.state.tab_mut(TabId::new(id)) {
            tab.loading = loading;
        }
    }
}

// ---------------------------------------------------------------------
// Omnibox
// ---------------------------------------------------------------------

// What the user typed, turned into a URL to load. Search terms become a
// search URL for the configured engine. Owned by the caller.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_omnibox_resolve(
    state: *const WegletState,
    input: *const c_char,
) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let Some(state) = (unsafe { as_ref(state) }) else {
        return into_c_string(String::new());
    };
    // SAFETY: contract of str_from_c.
    let Some(input) = (unsafe { str_from_c(input) }) else {
        return into_c_string(String::new());
    };

    let url = match weglet_core::parse_omnibox(input) {
        OmniboxAction::Navigate(url) => url,
        OmniboxAction::Search(query) => state.settings.search_engine.query_url(&query),
    };
    into_c_string(url)
}

// ---------------------------------------------------------------------
// Security
// ---------------------------------------------------------------------

// 0 = nothing to say, 1 = warn, 2 = block. Ints rather than an enum
// because an enum's layout across the boundary is one more thing to keep
// in step by hand.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_assess_navigation(url: *const c_char) -> u32 {
    // SAFETY: contract of str_from_c.
    let Some(url) = (unsafe { str_from_c(url) }) else {
        return 0;
    };
    match weglet_security::assess_navigation(url) {
        None => 0,
        Some(risk) => match risk.level {
            weglet_security::RiskLevel::Warning => 1,
            weglet_security::RiskLevel::Block => 2,
        },
    }
}

// The title of the last risk for `url`, or an empty string. Re-assesses
// rather than caching: holding the risk across the boundary would mean
// another lifetime for C++ to get wrong.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_risk_title(url: *const c_char) -> *mut c_char {
    // SAFETY: contract of str_from_c.
    let title = unsafe { str_from_c(url) }
        .and_then(weglet_security::assess_navigation)
        .map(|risk| risk.title)
        .unwrap_or_default();
    into_c_string(title)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_risk_reason(url: *const c_char) -> *mut c_char {
    // SAFETY: contract of str_from_c.
    let reason = unsafe { str_from_c(url) }
        .and_then(weglet_security::assess_navigation)
        .map(|risk| risk.reason)
        .unwrap_or_default();
    into_c_string(reason)
}

// True when the user's own block list covers this host.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_is_host_blocked(
    state: *const WegletState,
    host: *const c_char,
) -> bool {
    // SAFETY: contract of as_ref.
    let Some(state) = (unsafe { as_ref(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let Some(host) = (unsafe { str_from_c(host) }) else {
        return false;
    };
    weglet_security::matches_user_blocklist(host, &state.settings.blocked_hosts)
        || weglet_security::is_blocked_host(host)
}

// ---------------------------------------------------------------------
// Settings and session
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_terms_accepted(state: *const WegletState) -> bool {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.is_some_and(|state| state.settings.terms_accepted)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_accept_terms(state: *mut WegletState) {
    // SAFETY: contract of as_mut.
    if let Some(state) = unsafe { as_mut(state) } {
        state.settings.terms_accepted = true;
        state.persist_settings();
    }
}

// Writes the session to disk. Returns false when there was nothing to
// write to or the write failed -- the caller logs it, because losing the
// open tabs silently is how a user finds out the hard way.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_save_session(state: *const WegletState) -> bool {
    // SAFETY: contract of as_ref.
    let Some(state) = (unsafe { as_ref(state) }) else {
        return false;
    };
    if !state.settings.restore_session {
        return true;
    }
    let Some(paths) = &state.paths else {
        return false;
    };

    let session = Session {
        tabs: state
            .state
            .tabs()
            .iter()
            .map(|tab| SessionTab {
                entries: tab.history.entries().to_vec(),
                cursor: tab.history.cursor(),
            })
            .collect(),
        active: state.state.index_of(state.state.active_id()).unwrap_or(0),
    };
    session.save(&paths.session_file()).is_ok()
}

impl WegletState {
    fn persist_settings(&self) {
        if let Some(paths) = &self.paths {
            // Ignored deliberately: a failed settings write must not take
            // the browser down, and the next save will try again. The C++
            // side reports the profile directory being unwritable at
            // startup, which is the useful moment to say so.
            let _ = self.settings.save(&paths.settings_file());
        }
    }
}
