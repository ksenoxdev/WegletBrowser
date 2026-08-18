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

// Private except for `weglet_string_free`, re-exported below so
// tests/boundary.rs can bind it by the same name the browser links
// against. The module itself stays private: nothing outside this crate
// has business constructing the strings, only freeing them.
mod string;

pub use string::weglet_string_free;

use std::ffi::c_char;

use weglet_core::{AppState, OmniboxAction, TabId, WindowId};
use weglet_profile::{Paths, Session, SessionTab, SessionWindow, Settings};

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
    // Set when settings changed and have not reached the disk yet.
    //
    // Writing on every change meant an fsync -- see weglet-profile's
    // atomic::write, which syncs before it renames, and has to -- on the
    // browser's UI thread, inside the click that toggled a setting.
    // Marking instead and letting the browser flush on its own schedule
    // keeps the atomicity and takes the disk out of the input path. The
    // flush still happens: weglet_flush_settings on a timer, and again at
    // shutdown, so nothing is lost by deferring it.
    settings_dirty: bool,
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
            .map(|session| AppState::restore(session.to_restore_input()))
            .unwrap_or_default()
    } else {
        AppState::new()
    };

    // Optional per-profile data files, applied before anything asks a
    // question that depends on them. Absent is the normal case and means
    // the compiled-in table stands; present means the user wants their
    // own list, and it replaces rather than extends -- someone who
    // supplies a list has a reason, and silently keeping entries they
    // removed would defeat it.
    //
    // A malformed file is reported and ignored rather than fatal: a bad
    // list must not be the reason the browser will not start.
    if let Some(paths) = &paths {
        load_overrides(paths);
    }

    Box::into_raw(Box::new(WegletState {
        state,
        settings,
        paths,
        settings_dirty: false,
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
// Windows
// ---------------------------------------------------------------------

// Every tab question below takes the window it is about. The model used
// to hold one flat tab list and one active tab, so a second window would
// have shown the same tabs and fought over which was in front -- while the
// C++ side already counted windows and quit when the last one closed.

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_window_count(state: *const WegletState) -> usize {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(0, |state| state.state.windows().len())
}

// The id at `index`, or 0 when out of range. Ids start at 0 too, so
// callers read the count first.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_window_id_at(state: *const WegletState, index: usize) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.windows().get(index))
        .map_or(0, |window| window.id.value())
}

// The new window's id, or 0 when the ceiling is reached. It starts with
// one blank tab, the same way the first window does.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_open_window(state: *mut WegletState) -> u64 {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }
        .and_then(|state| state.state.open_window())
        .map_or(0, |id| id.value())
}

// Its tabs go with it. Closing the last window leaves a fresh one, so the
// model always has something a window could show.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_close_window(state: *mut WegletState, window: u64) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }
        .is_some_and(|state| state.state.close_window(WindowId::new(window)))
}

// Which window a tab is in. 0 is a real window id, so callers that need
// to tell "window 0" from "no such tab" ask about the tab first.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_window(state: *const WegletState, id: u64) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.tab(TabId::new(id)))
        .map_or(0, |tab| tab.window.value())
}

// ---------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_count(state: *const WegletState, window: u64) -> usize {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(0, |state| {
        state.state.tab_count_in(WindowId::new(window))
    })
}

// The id at `index`, or 0 if out of range. Ids start at 0 too, so callers
// check the count first -- which is why this pairs with weglet_tab_count.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_id_at(
    state: *const WegletState,
    window: u64,
    index: usize,
) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.tab_in_at(WindowId::new(window), index))
        .map_or(0, |tab| tab.id.value())
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_active_tab_id(state: *const WegletState, window: u64) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.active_id(WindowId::new(window)))
        .map_or(0, |id| id.value())
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

// Whether this tab is loading. The model has kept this since
// weglet_tab_loading_changed existed; nothing could read it, so the tab
// strip had a spinner in its CSS and no way to know when to show it.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_tab_loading(state: *const WegletState, id: u64) -> bool {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }
        .and_then(|state| state.state.tab(TabId::new(id)))
        .is_some_and(|tab| tab.loading)
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
pub unsafe extern "C" fn weglet_open_tab(
    state: *mut WegletState,
    window: u64,
    url: *const c_char,
) -> u64 {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return 0;
    };
    // SAFETY: contract of str_from_c.
    let url = unsafe { str_from_c(url) }.unwrap_or(weglet_core::BLANK_TAB);
    state
        .state
        .open_tab(WindowId::new(window), url)
        .map_or(0, |id| id.value())
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
pub unsafe extern "C" fn weglet_cycle_tab(state: *mut WegletState, window: u64, forward: bool) {
    // SAFETY: contract of as_mut.
    if let Some(state) = unsafe { as_mut(state) } {
        state.state.cycle(WindowId::new(window), forward);
    }
}

// One-based, as on the keyboard. 9 means the last tab.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_activate_tab_at(
    state: *mut WegletState,
    window: u64,
    position: usize,
) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }
        .is_some_and(|state| state.state.activate_by_position(WindowId::new(window), position))
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
        OmniboxAction::Search(query) => weglet_profile::query_url(
            &state.settings.search_engine,
            &query,
            &state.settings.custom_search_url,
        )
        // A configured-but-broken custom engine, or an id an override
        // removed, falls back to the default rather than returning
        // nothing -- Enter in the address bar has to go somewhere.
        .unwrap_or_else(|| {
            weglet_profile::query_url(weglet_profile::default_engine_id(), &query, "")
                .expect("the default engine always produces a URL")
        }),
    };
    into_c_string(url)
}

// ---------------------------------------------------------------------
// Security
// ---------------------------------------------------------------------

// One assessment, one call.
//
// 0 = nothing to say, 1 = warn, 2 = block. Ints rather than an enum
// because an enum's layout across the boundary is one more thing to keep
// in step by hand.
//
// `title`, `reason` and `host` may each be null when the caller does not
// want that part. Any non-null one is filled with an owned string the
// caller frees with weglet_string_free -- including when the answer is 0,
// where all three are empty strings rather than untouched, so a caller
// cannot read whatever the pointer happened to hold.
//
// The four separate entry points this replaced each re-ran the whole
// assessment from the raw URL -- punycode, the public suffix list, the
// skeleton fold and every brand rule, four times for one navigation.
// That was to avoid a risk object living across the boundary; filling
// out-params costs no lifetime and does the work once. It matters now
// that assessment is on the navigation path rather than only behind the
// address bar.
//
// The state is not consulted here: this is a pure function of the URL.
// The user's own block list is a separate question -- see
// weglet_is_host_blocked.
///
/// # Safety
///
/// `url` must be null or NUL-terminated and valid for the call. Each of
/// `title`, `reason` and `host` must be null or point at a writable
/// `char*`. Every string written is owned by the caller and freed with
/// `weglet_string_free`.
#[no_mangle]
pub unsafe extern "C" fn weglet_assess_navigation(
    url: *const c_char,
    title: *mut *mut c_char,
    reason: *mut *mut c_char,
    host: *mut *mut c_char,
) -> u32 {
    // SAFETY: each pointer is null-checked before it is written, and the
    // caller's contract says a non-null one is writable.
    let write = |slot: *mut *mut c_char, value: String| {
        if !slot.is_null() {
            unsafe { *slot = into_c_string(value) };
        }
    };

    // SAFETY: contract of str_from_c.
    let risk = unsafe { str_from_c(url) }.and_then(weglet_security::assess_navigation);

    let Some(risk) = risk else {
        write(title, String::new());
        write(reason, String::new());
        write(host, String::new());
        return 0;
    };

    let level = match risk.level {
        weglet_security::RiskLevel::Warning => 1,
        weglet_security::RiskLevel::Block => 2,
    };
    write(title, risk.title);
    write(reason, risk.reason);
    // Empty when the assessment has no host to show -- an unparseable
    // URL, for instance. The target URL is not returned: the caller
    // already has it, since it is what was passed in.
    write(host, risk.normalized_host.unwrap_or_default());
    level
}

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
    is_blocked(state, host)
}

// The same question asked about a whole URL.
//
// Exists because the caller that matters is the navigation path, which
// holds a URL and not a host, and pulling the host out on the C++ side
// would put a second host parser next to weglet-url's -- the one that
// already knows a backslash ends the authority and that the host is what
// follows the *last* '@'. Getting that wrong on this particular path
// means "google.com@evil.example" is checked as google.com.
//
// False for anything with no host at all (about:blank, chrome://weglet/):
// there is nothing to match a block list against, and a page of ours must
// never be blockable by an entry the user typed.
// The same question about a whole URL, with the wording for the notice.
//
// `title` and `reason` may each be null; a non-null one is always written,
// empty when the answer is false. The wording lives in weglet-security
// with every other string the notice page shows.
///
/// # Safety
///
/// `url` must be null or NUL-terminated. `title` and `reason` must each be
/// null or point at a writable `char*`; any string written is owned by the
/// caller and freed with `weglet_string_free`.
#[no_mangle]
pub unsafe extern "C" fn weglet_is_url_blocked(
    state: *const WegletState,
    url: *const c_char,
    title: *mut *mut c_char,
    reason: *mut *mut c_char,
) -> bool {
    // SAFETY: each pointer is null-checked before it is written.
    let write = |slot: *mut *mut c_char, value: &str| {
        if !slot.is_null() {
            unsafe { *slot = into_c_string(value.to_string()) };
        }
    };

    // SAFETY: contract of as_ref and str_from_c.
    let blocked = unsafe { as_ref(state) }.is_some_and(|state| {
        unsafe { str_from_c(url) }
            .and_then(weglet_url::host)
            .is_some_and(|host| is_blocked(state, host))
    });

    // Written either way, like weglet_assess_navigation, so a caller
    // cannot read whatever the pointer held before the call.
    if blocked {
        write(title, weglet_security::USER_BLOCK_TITLE);
        write(reason, weglet_security::USER_BLOCK_REASON);
    } else {
        write(title, "");
        write(reason, "");
    }
    blocked
}

fn is_blocked(state: &WegletState, host: &str) -> bool {
    weglet_security::matches_user_blocklist(host, &state.settings.blocked_hosts)
        || weglet_security::is_blocked_host(host)
}

// ---------------------------------------------------------------------
// Shortcuts
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_shortcut_count(state: *const WegletState) -> usize {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(0, |state| state.settings.shortcuts.len())
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_shortcut_title(
    state: *const WegletState,
    index: usize,
) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let title = unsafe { as_ref(state) }
        .and_then(|state| state.settings.shortcuts.get(index))
        .map(|shortcut| shortcut.title.clone())
        .unwrap_or_default();
    into_c_string(title)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_shortcut_url(
    state: *const WegletState,
    index: usize,
) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let url = unsafe { as_ref(state) }
        .and_then(|state| state.settings.shortcuts.get(index))
        .map(|shortcut| shortcut.url.clone())
        .unwrap_or_default();
    into_c_string(url)
}

// False when the dock is full, so the caller can say so rather than leaving
// the user wondering why nothing happened.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_add_shortcut(
    state: *mut WegletState,
    title: *const c_char,
    url: *const c_char,
) -> bool {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let (Some(title), Some(url)) =
        (unsafe { str_from_c(title) }, unsafe { str_from_c(url) })
    else {
        return false;
    };
    // The URL is stored as typed and resolved when the tile is clicked, the
    // same way an address bar entry is -- so "example.com" works here too.
    if !state.settings.add_shortcut(title, url) {
        return false;
    }
    state.persist_settings();
    true
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_edit_shortcut(
    state: *mut WegletState,
    index: usize,
    title: *const c_char,
    url: *const c_char,
) -> bool {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let (Some(title), Some(url)) =
        (unsafe { str_from_c(title) }, unsafe { str_from_c(url) })
    else {
        return false;
    };
    if !state.settings.edit_shortcut(index, title, url) {
        return false;
    }
    state.persist_settings();
    true
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_remove_shortcut(
    state: *mut WegletState,
    index: usize,
) -> bool {
    // SAFETY: contract of as_mut.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    if !state.settings.remove_shortcut(index) {
        return false;
    }
    state.persist_settings();
    true
}

// The line under the new tab page's search field. Composed here because the
// engine is a profile setting and the page does not read the profile.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_new_tab_hint(state: *const WegletState) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let hint = unsafe { as_ref(state) }
        .map(|state| {
            let engine = weglet_profile::engine_label(&state.settings.search_engine);
            format!("Searches go to {engine}")
        })
        .unwrap_or_default();
    into_c_string(hint)
}

// ---------------------------------------------------------------------
// Settings: search
// ---------------------------------------------------------------------

// "duckduckgo" | "google" | "bing" | "custom". A string rather than an int:
// the settings page has to display and compare it, and a string it can log
// unrecognised is safer than an int it would have to guess the meaning of.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_search_engine(state: *const WegletState) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let id = unsafe { as_ref(state) }
        .map(|state| state.settings.search_engine.clone())
        .unwrap_or_default();
    into_c_string(id)
}


// False for an id the browser does not recognise, which is what a settings
// page from a newer version sending a value this one does not know looks
// like -- the setting is left as it was rather than reset to a default.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_set_search_engine(
    state: *mut WegletState,
    id: *const c_char,
) -> bool {
    // SAFETY: contract of as_mut and str_from_c.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let Some(id) = (unsafe { str_from_c(id) }) else {
        return false;
    };
    // Checked against the table rather than a match arm per engine: the
    // table is data now, and an id it does not contain is a page sending
    // something no settings screen offered.
    if !weglet_profile::is_known_engine(id) {
        return false;
    }
    state.settings.search_engine = id.to_string();
    state.persist_settings();
    true
}

// The number of engines the settings page can offer. Read this and then
// weglet_engine_id_at / weglet_engine_label_at for 0..count -- the engine
// list lives in weglet-profile's data/engines.toml, and this is what lets
// the settings page build its own list from it instead of holding a
// second copy. "custom" is not in it: it is not a choice on the list, it
// is what the template box means.
#[no_mangle]
pub extern "C" fn weglet_engine_count() -> usize {
    weglet_profile::engines().len()
}


#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_engine_id_at(index: usize) -> *mut c_char {
    let id = weglet_profile::engines()
        .get(index)
        .map(|engine| engine.id.clone())
        .unwrap_or_default();
    into_c_string(id)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_engine_label_at(index: usize) -> *mut c_char {
    let label = weglet_profile::engines()
        .get(index)
        .map(|engine| engine.label.clone())
        .unwrap_or_default();
    into_c_string(label)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_custom_search_url(state: *const WegletState) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let url = unsafe { as_ref(state) }
        .map(|state| state.settings.custom_search_url.clone())
        .unwrap_or_default();
    into_c_string(url)
}

// Accepted even when it will not validate as a template yet: the user is
// often mid-edit, and rejecting every keystroke that is not yet a complete
// %s URL would make the field impossible to type into. Validated instead at
// the point it is used, in weglet_omnibox_resolve.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_set_custom_search_url(
    state: *mut WegletState,
    url: *const c_char,
) {
    // SAFETY: contract of as_mut.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return;
    };
    // SAFETY: contract of str_from_c.
    let Some(url) = (unsafe { str_from_c(url) }) else {
        return;
    };
    state.settings.custom_search_url = url.to_string();
    state.persist_settings();
}

// ---------------------------------------------------------------------
// Settings: general
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_restore_session(state: *const WegletState) -> bool {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.is_some_and(|state| state.settings.restore_session)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_set_restore_session(state: *mut WegletState, on: bool) {
    // SAFETY: contract of as_mut.
    if let Some(state) = unsafe { as_mut(state) } {
        state.settings.restore_session = on;
        state.persist_settings();
    }
}

// ---------------------------------------------------------------------
// Settings: appearance
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_accent_color(state: *const WegletState) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let color = unsafe { as_ref(state) }
        .map(|state| state.settings.accent_color.clone())
        .unwrap_or_default();
    into_c_string(color)
}

// False for anything that is not #RRGGBB. The colour is never left as
// something the UI cannot turn into a swatch.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_set_accent_color(
    state: *mut WegletState,
    color: *const c_char,
) -> bool {
    // SAFETY: contract of as_mut.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let Some(color) = (unsafe { str_from_c(color) }) else {
        return false;
    };
    if !state.settings.set_accent_color(color) {
        return false;
    }
    state.persist_settings();
    true
}

// "pill" | "rounded" | "square".
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_address_bar_shape(state: *const WegletState) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let shape = unsafe { as_ref(state) }
        .map(|state| address_bar_shape_id(state.settings.address_bar_shape))
        .unwrap_or("pill");
    into_c_string(shape.to_string())
}

fn address_bar_shape_id(shape: weglet_profile::AddressBarShape) -> &'static str {
    match shape {
        weglet_profile::AddressBarShape::Pill => "pill",
        weglet_profile::AddressBarShape::Rounded => "rounded",
        weglet_profile::AddressBarShape::Square => "square",
    }
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_set_address_bar_shape(
    state: *mut WegletState,
    shape: *const c_char,
) -> bool {
    // SAFETY: contract of as_mut.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let parsed = match unsafe { str_from_c(shape) } {
        Some("pill") => weglet_profile::AddressBarShape::Pill,
        Some("rounded") => weglet_profile::AddressBarShape::Rounded,
        Some("square") => weglet_profile::AddressBarShape::Square,
        _ => return false,
    };
    state.settings.address_bar_shape = parsed;
    state.persist_settings();
    true
}

// ---------------------------------------------------------------------
// Settings: blocked hosts
// ---------------------------------------------------------------------

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_blocked_host_count(state: *const WegletState) -> usize {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(0, |state| state.settings.blocked_hosts.len())
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_blocked_host_at(
    state: *const WegletState,
    index: usize,
) -> *mut c_char {
    // SAFETY: contract of as_ref.
    let host = unsafe { as_ref(state) }
        .and_then(|state| state.settings.blocked_hosts.get(index))
        .cloned()
        .unwrap_or_default();
    into_c_string(host)
}

// Canonicalised before storing -- see weglet_security::canonical_host --
// so "EXAMPLE.com." and "example.com" end up as the one entry rather than
// two that silently disagree about whether the site is blocked.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_block_host(
    state: *mut WegletState,
    host: *const c_char,
) -> bool {
    // SAFETY: contract of as_mut.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    // SAFETY: contract of str_from_c.
    let Some(host) = (unsafe { str_from_c(host) }) else {
        return false;
    };
    let canonical = weglet_security::canonical_host(host);
    if canonical.is_empty() || state.settings.blocked_hosts.iter().any(|h| h == &canonical) {
        return false;
    }
    state.settings.blocked_hosts.push(canonical);
    state.persist_settings();
    true
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_unblock_host(state: *mut WegletState, index: usize) -> bool {
    // SAFETY: contract of as_mut.
    let Some(state) = (unsafe { as_mut(state) }) else {
        return false;
    };
    if index >= state.settings.blocked_hosts.len() {
        return false;
    }
    state.settings.blocked_hosts.remove(index);
    state.persist_settings();
    true
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
        windows: state
            .state
            .windows()
            .iter()
            .map(|window| SessionWindow {
                tabs: state
                    .state
                    .tabs_in(window.id)
                    .map(|tab| SessionTab {
                        entries: tab.history.entries().to_vec(),
                        cursor: tab.history.cursor(),
                    })
                    .collect(),
                active: state
                    .state
                    .position_in_window(window.active())
                    .unwrap_or(0),
            })
            .collect(),
        ..Session::default()
    };
    session.save(&paths.session_file()).is_ok()
}

// Reads the four optional files a profile may carry. Errors are dropped
// on purpose: weglet_state_new has no way to report them and no caller
// that could act on one. What a user gets for a malformed file is the
// built-in list, which is the safe answer.
fn load_overrides(paths: &Paths) {
    if let Ok(text) = std::fs::read_to_string(paths.blocklist_file()) {
        weglet_security::set_blocklist_override(&text);
    }
    if let Ok(text) = std::fs::read_to_string(paths.brands_file()) {
        let _ = weglet_security::set_brand_rules_override(&text);
    }
    if let Ok(text) = std::fs::read_to_string(paths.sensitive_words_file()) {
        weglet_security::set_sensitive_words_override(&text);
    }
    if let Ok(text) = std::fs::read_to_string(paths.engines_file()) {
        let _ = weglet_profile::set_engines_override(&text);
    }
}

impl WegletState {
    // Marks settings as needing a write. Does not touch the disk: see
    // `settings_dirty`.
    fn persist_settings(&mut self) {
        self.settings_dirty = true;
    }

    fn flush_settings(&mut self) -> bool {
        if !self.settings_dirty {
            return true;
        }
        let Some(paths) = &self.paths else {
            // Nowhere to write. Left dirty rather than cleared, so a
            // later call still reports the failure instead of claiming
            // the settings are safe.
            return false;
        };
        match self.settings.save(&paths.settings_file()) {
            Ok(()) => {
                self.settings_dirty = false;
                true
            }
            // Left dirty on failure so the next flush retries. The caller
            // reports it -- a settings file that silently stops being
            // written looks exactly like one that is being written.
            Err(_) => false,
        }
    }
}

// Writes any pending settings change. False when there was something to
// write and it could not be written.
//
// Called by the browser on a timer and again at shutdown; nothing else in
// this file touches the disk for settings. Cheap and a no-op when nothing
// changed, so calling it often costs nothing.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_flush_settings(state: *mut WegletState) -> bool {
    // SAFETY: contract of as_mut.
    unsafe { as_mut(state) }.is_some_and(|state| state.flush_settings())
}

// True when settings have changed and not yet been written. Lets the
// browser skip a timer tick rather than call across the boundary for
// nothing.
// How often the browser should call weglet_flush_settings and
// weglet_save_session, in seconds. From the profile, clamped on load, so
// a machine with a slow disk or one that gets closed abruptly can be
// tuned without a rebuild.
#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_settings_flush_seconds(state: *const WegletState) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(5, |state| state.settings.settings_flush_seconds)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_session_save_seconds(state: *const WegletState) -> u64 {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.map_or(30, |state| state.settings.session_save_seconds)
}

#[doc = safety_doc!()]
#[no_mangle]
pub unsafe extern "C" fn weglet_settings_dirty(state: *const WegletState) -> bool {
    // SAFETY: contract of as_ref.
    unsafe { as_ref(state) }.is_some_and(|state| state.settings_dirty)
}
