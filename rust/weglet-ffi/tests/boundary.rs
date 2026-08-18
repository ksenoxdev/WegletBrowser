// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-ffi/tests/boundary.rs
//
// The boundary driven the way C++ drives it: create a state, call entry
// points on it in sequence, free it.
//
// The unit tests inside the crate cover the string helpers. Nothing
// covered the entry points themselves, which is where a mistake is
// undefined behaviour in the browser process rather than a failed
// assertion -- a returned pointer never freed, an index read past the
// end, an out-param left holding whatever it held before.
//
// Every test here goes through `guard()`. The profile path comes from an
// environment variable, and cargo runs tests as threads in one process,
// so two of them setting it at once would have each other's directory.

use std::ffi::{c_char, CStr, CString};
use std::sync::{Mutex, MutexGuard, OnceLock};

// The public surface, bound exactly as weglet_ffi.h declares it.
//
// Each constant below is the real function stored in a variable whose
// type is written out by hand from the header. A signature that drifts
// from the header is a compile error here, and calling through these
// bindings is also what makes the linker keep the `#[no_mangle]` symbols
// the browser will look for.
#[allow(non_snake_case, non_upper_case_globals)]
mod abi {
    use std::ffi::c_char;
    use weglet_ffi::WegletState;

    pub const state_new: extern "C" fn() -> *mut WegletState = weglet_ffi::weglet_state_new;
    pub const state_free: unsafe extern "C" fn(*mut WegletState) = weglet_ffi::weglet_state_free;
    pub const string_free: unsafe extern "C" fn(*mut c_char) = weglet_ffi::weglet_string_free;

    pub const window_count: unsafe extern "C" fn(*const WegletState) -> usize =
        weglet_ffi::weglet_window_count;
    pub const window_id_at: unsafe extern "C" fn(*const WegletState, usize) -> u64 =
        weglet_ffi::weglet_window_id_at;
    pub const open_window: unsafe extern "C" fn(*mut WegletState) -> u64 =
        weglet_ffi::weglet_open_window;
    pub const close_window: unsafe extern "C" fn(*mut WegletState, u64) -> bool =
        weglet_ffi::weglet_close_window;
    pub const tab_window: unsafe extern "C" fn(*const WegletState, u64) -> u64 =
        weglet_ffi::weglet_tab_window;

    pub const tab_count: unsafe extern "C" fn(*const WegletState, u64) -> usize =
        weglet_ffi::weglet_tab_count;
    pub const tab_id_at: unsafe extern "C" fn(*const WegletState, u64, usize) -> u64 =
        weglet_ffi::weglet_tab_id_at;
    pub const active_tab_id: unsafe extern "C" fn(*const WegletState, u64) -> u64 =
        weglet_ffi::weglet_active_tab_id;
    pub const tab_url: unsafe extern "C" fn(*const WegletState, u64) -> *mut c_char =
        weglet_ffi::weglet_tab_url;
    pub const tab_label: unsafe extern "C" fn(*const WegletState, u64) -> *mut c_char =
        weglet_ffi::weglet_tab_label;
    pub const open_tab: unsafe extern "C" fn(*mut WegletState, u64, *const c_char) -> u64 =
        weglet_ffi::weglet_open_tab;
    pub const close_tab: unsafe extern "C" fn(*mut WegletState, u64) -> bool =
        weglet_ffi::weglet_close_tab;
    pub const tab_navigated: unsafe extern "C" fn(*mut WegletState, u64, *const c_char) =
        weglet_ffi::weglet_tab_navigated;
    pub const tab_go_back: unsafe extern "C" fn(*mut WegletState, u64) -> *mut c_char =
        weglet_ffi::weglet_tab_go_back;

    pub const omnibox_resolve: unsafe extern "C" fn(
        *const WegletState,
        *const c_char,
    ) -> *mut c_char = weglet_ffi::weglet_omnibox_resolve;

    pub const assess_navigation: unsafe extern "C" fn(
        *const c_char,
        *mut *mut c_char,
        *mut *mut c_char,
        *mut *mut c_char,
    ) -> u32 = weglet_ffi::weglet_assess_navigation;

    pub const block_host: unsafe extern "C" fn(*mut WegletState, *const c_char) -> bool =
        weglet_ffi::weglet_block_host;
    pub const unblock_host: unsafe extern "C" fn(*mut WegletState, usize) -> bool =
        weglet_ffi::weglet_unblock_host;
    pub const blocked_host_count: unsafe extern "C" fn(*const WegletState) -> usize =
        weglet_ffi::weglet_blocked_host_count;
    pub const blocked_host_at: unsafe extern "C" fn(*const WegletState, usize) -> *mut c_char =
        weglet_ffi::weglet_blocked_host_at;
    pub const is_url_blocked: unsafe extern "C" fn(
        *const WegletState,
        *const c_char,
        *mut *mut c_char,
        *mut *mut c_char,
    ) -> bool = weglet_ffi::weglet_is_url_blocked;
    pub const is_host_blocked: unsafe extern "C" fn(*const WegletState, *const c_char) -> bool =
        weglet_ffi::weglet_is_host_blocked;

    pub const set_accent_color: unsafe extern "C" fn(*mut WegletState, *const c_char) -> bool =
        weglet_ffi::weglet_set_accent_color;
    pub const accent_color: unsafe extern "C" fn(*const WegletState) -> *mut c_char =
        weglet_ffi::weglet_accent_color;
    pub const settings_dirty: unsafe extern "C" fn(*const WegletState) -> bool =
        weglet_ffi::weglet_settings_dirty;
    pub const flush_settings: unsafe extern "C" fn(*mut WegletState) -> bool =
        weglet_ffi::weglet_flush_settings;

    pub const engine_count: extern "C" fn() -> usize = weglet_ffi::weglet_engine_count;
    pub const engine_id_at: unsafe extern "C" fn(usize) -> *mut c_char =
        weglet_ffi::weglet_engine_id_at;
    pub const set_search_engine: unsafe extern "C" fn(*mut WegletState, *const c_char) -> bool =
        weglet_ffi::weglet_set_search_engine;
}

fn lock() -> &'static Mutex<()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(()))
}

struct Fixture {
    _guard: MutexGuard<'static, ()>,
    dir: std::path::PathBuf,
}

impl Drop for Fixture {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.dir);
    }
}

// A private profile directory, held for the duration of one test.
fn guard(name: &str) -> Fixture {
    let mutex = lock();
    // A panicking test must not make every later one fail on a poisoned
    // lock -- the directory is fresh either way.
    let g = mutex.lock().unwrap_or_else(|e| e.into_inner());
    let dir = std::env::temp_dir().join(format!("weglet-ffi-{name}"));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    std::env::set_var("XDG_DATA_HOME", &dir);
    std::env::set_var("LOCALAPPDATA", &dir);
    Fixture { _guard: g, dir }
}

// Takes ownership the way WegletBridge::TakeString does, so a leak here
// would be a leak there.
fn take(raw: *mut c_char) -> String {
    assert!(!raw.is_null(), "the boundary must never return null");
    let value = unsafe { CStr::from_ptr(raw) }.to_string_lossy().into_owned();
    unsafe { abi::string_free(raw) };
    value
}

fn c(value: &str) -> CString {
    CString::new(value).unwrap()
}

#[test]
fn a_fresh_state_has_one_blank_tab() {
    let _f = guard("fresh");
    let state = abi::state_new();
    let w = unsafe { abi::window_id_at(state, 0) };
    assert!(!state.is_null());
    assert_eq!(unsafe { abi::tab_count(state, w) }, 1);
    let id = unsafe { abi::active_tab_id(state, w) };
    assert_eq!(take(unsafe { abi::tab_url(state, id) }), "about:blank");
    assert_eq!(take(unsafe { abi::tab_label(state, id) }), "New Tab");
    unsafe { abi::state_free(state) };
}

#[test]
fn tabs_open_navigate_and_close() {
    let _f = guard("tabs");
    let state = abi::state_new();
    let w = unsafe { abi::window_id_at(state, 0) };

    let id = unsafe { abi::open_tab(state, w, c("https://a.example").as_ptr()) };
    assert_ne!(id, 0);
    assert_eq!(unsafe { abi::tab_count(state, w) }, 2);
    assert_eq!(unsafe { abi::active_tab_id(state, w) }, id);

    unsafe { abi::tab_navigated(state, id, c("https://b.example").as_ptr()) };
    assert_eq!(take(unsafe { abi::tab_url(state, id) }), "https://b.example");
    assert_eq!(
        take(unsafe { abi::tab_go_back(state, id) }),
        "https://a.example"
    );

    assert!(unsafe { abi::close_tab(state, id) });
    assert_eq!(unsafe { abi::tab_count(state, w) }, 1);
    unsafe { abi::state_free(state) };
}

// Every one of these would be a read past the end if the entry point
// trusted its argument.
#[test]
fn out_of_range_indices_are_answered_not_trusted() {
    let _f = guard("range");
    let state = abi::state_new();
    let w = unsafe { abi::window_id_at(state, 0) };
    assert_eq!(unsafe { abi::tab_id_at(state, w, 999) }, 0);
    assert_eq!(take(unsafe { abi::tab_url(state, 999) }), "");
    assert_eq!(take(unsafe { abi::tab_label(state, 999) }), "");
    assert_eq!(take(unsafe { abi::blocked_host_at(state, 999) }), "");
    assert!(!unsafe { abi::close_tab(state, 999) });
    assert!(!unsafe { abi::unblock_host(state, 999) });
    assert_eq!(take(unsafe { abi::engine_id_at(999) }), "");
    unsafe { abi::state_free(state) };
}

// A null handle is C++ having a bug, and must not be this side crashing.
#[test]
fn a_null_handle_is_a_defined_no_op() {
    let _f = guard("null");
    let null = std::ptr::null_mut();
    assert_eq!(unsafe { abi::tab_count(null, 0) }, 0);
    assert_eq!(unsafe { abi::active_tab_id(null, 0) }, 0);
    assert_eq!(take(unsafe { abi::tab_url(null, 0) }), "");
    assert_eq!(unsafe { abi::open_tab(null, 0, c("https://a.example").as_ptr()) }, 0);
    assert!(!unsafe { abi::is_url_blocked(null, c("https://a.example").as_ptr(), std::ptr::null_mut(), std::ptr::null_mut()) });
    assert_eq!(take(unsafe { abi::omnibox_resolve(null, c("x").as_ptr()) }), "");
    unsafe { abi::state_free(null) };
}

#[test]
fn the_omnibox_resolves_through_the_configured_engine() {
    let _f = guard("omnibox");
    let state = abi::state_new();
    assert_eq!(
        take(unsafe { abi::omnibox_resolve(state, c("example.com").as_ptr()) }),
        "https://example.com"
    );
    assert_eq!(
        take(unsafe { abi::omnibox_resolve(state, c("rust browser").as_ptr()) }),
        "https://duckduckgo.com/?q=rust%20browser"
    );

    // Every id the engine list offers has to be one set_search_engine
    // accepts, or the settings page can offer a choice that does nothing.
    for index in 0..abi::engine_count() {
        let id = take(unsafe { abi::engine_id_at(index) });
        assert!(
            unsafe { abi::set_search_engine(state, c(&id).as_ptr()) },
            "engine {id} is offered but not accepted"
        );
    }
    assert!(!unsafe { abi::set_search_engine(state, c("nonesuch").as_ptr()) });
    unsafe { abi::state_free(state) };
}

#[test]
fn one_assessment_fills_every_out_param() {
    let _f = guard("risk");
    let (mut title, mut reason, mut host) =
        (std::ptr::null_mut(), std::ptr::null_mut(), std::ptr::null_mut());

    let level = unsafe {
        abi::assess_navigation(
            c("https://gooogle.com/").as_ptr(),
            &mut title,
            &mut reason,
            &mut host,
        )
    };
    assert_eq!(level, 2);
    assert!(take(title).contains("Google"));
    assert!(!take(reason).is_empty());
    assert_eq!(take(host), "gooogle.com");
}

// The out-params are written even when there is no risk, so a caller
// cannot read whatever the pointer held before the call.
#[test]
fn a_safe_url_still_writes_empty_strings() {
    let _f = guard("risk-none");
    let (mut title, mut reason, mut host) =
        (std::ptr::null_mut(), std::ptr::null_mut(), std::ptr::null_mut());
    let level = unsafe {
        abi::assess_navigation(
            c("https://example.com/").as_ptr(),
            &mut title,
            &mut reason,
            &mut host,
        )
    };
    assert_eq!(level, 0);
    assert_eq!(take(title), "");
    assert_eq!(take(reason), "");
    assert_eq!(take(host), "");
}

// A caller that wants only the level passes nothing to fill.
#[test]
fn null_out_params_are_allowed() {
    let _f = guard("risk-null");
    let level = unsafe {
        abi::assess_navigation(
            c("https://gooogle.com/").as_ptr(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
        )
    };
    assert_eq!(level, 2);
}

#[test]
fn a_blocked_host_is_blocked_however_the_url_spells_it() {
    let _f = guard("block");
    let state = abi::state_new();

    assert!(unsafe { abi::block_host(state, c("EVIL.example.").as_ptr()) });
    assert_eq!(unsafe { abi::blocked_host_count(state) }, 1);
    // Stored canonicalised, so the settings page shows one spelling.
    assert_eq!(take(unsafe { abi::blocked_host_at(state, 0) }), "evil.example");
    // The same host twice is not two entries.
    assert!(!unsafe { abi::block_host(state, c("evil.example").as_ptr()) });

    for url in [
        "https://evil.example/",
        "https://sub.evil.example/path",
        "http://EVIL.EXAMPLE:8080/",
        // The host is what follows the last '@', so this is evil.example
        // and not example.com.
        "https://example.com@evil.example/",
    ] {
        assert!(
            unsafe { abi::is_url_blocked(state, c(url).as_ptr(), std::ptr::null_mut(), std::ptr::null_mut()) },
            "{url} should be blocked"
        );
    }
    assert!(!unsafe { abi::is_url_blocked(state, c("https://example.com/").as_ptr(), std::ptr::null_mut(), std::ptr::null_mut()) });

    // Nothing without a host can be blocked, or a page of ours could be.
    for url in ["about:blank", "chrome://weglet/settings.html", ""] {
        assert!(
            !unsafe { abi::is_url_blocked(state, c(url).as_ptr(), std::ptr::null_mut(), std::ptr::null_mut()) },
            "{url} has no host and must not match"
        );
    }

    // The wording comes back with the answer, so the notice page and the
    // heuristics are fed from one place.
    let (mut title, mut reason) = (std::ptr::null_mut(), std::ptr::null_mut());
    assert!(unsafe {
        abi::is_url_blocked(
            state,
            c("https://evil.example/").as_ptr(),
            &mut title,
            &mut reason,
        )
    });
    assert!(!take(title).is_empty());
    assert!(take(reason).contains("block list"));

    // And empty rather than untouched when nothing is blocked.
    let (mut title, mut reason) = (std::ptr::null_mut(), std::ptr::null_mut());
    assert!(!unsafe {
        abi::is_url_blocked(
            state,
            c("https://example.com/").as_ptr(),
            &mut title,
            &mut reason,
        )
    });
    assert_eq!(take(title), "");
    assert_eq!(take(reason), "");

    assert!(unsafe { abi::unblock_host(state, 0) });
    assert!(!unsafe { abi::is_url_blocked(state, c("https://evil.example/").as_ptr(), std::ptr::null_mut(), std::ptr::null_mut()) });

    // The built-in list is in force regardless of the user's.
    assert!(unsafe { abi::is_host_blocked(state, c("stats.doubleclick.net").as_ptr()) });
    unsafe { abi::state_free(state) };
}

#[test]
fn windows_hold_their_own_tabs_and_their_own_active_one() {
    let _f = guard("windows");
    let state = abi::state_new();
    let first = unsafe { abi::window_id_at(state, 0) };
    assert_eq!(unsafe { abi::window_count(state) }, 1);

    let second = unsafe { abi::open_window(state) };
    assert_ne!(second, first);
    assert_eq!(unsafe { abi::window_count(state) }, 2);

    let a = unsafe { abi::open_tab(state, first, c("https://a.example").as_ptr()) };
    let b = unsafe { abi::open_tab(state, second, c("https://b.example").as_ptr()) };

    assert_eq!(unsafe { abi::active_tab_id(state, first) }, a);
    assert_eq!(unsafe { abi::active_tab_id(state, second) }, b);
    assert_eq!(unsafe { abi::tab_count(state, first) }, 2);
    assert_eq!(unsafe { abi::tab_count(state, second) }, 2);
    assert_eq!(unsafe { abi::tab_window(state, a) }, first);
    assert_eq!(unsafe { abi::tab_window(state, b) }, second);

    // Closing one takes its tabs and leaves the other alone.
    assert!(unsafe { abi::close_window(state, second) });
    assert_eq!(unsafe { abi::window_count(state) }, 1);
    assert_eq!(unsafe { abi::tab_count(state, first) }, 2);
    assert_eq!(unsafe { abi::active_tab_id(state, first) }, a);

    // An unknown window answers rather than reaching past the end.
    assert_eq!(unsafe { abi::tab_count(state, 9999) }, 0);
    assert_eq!(unsafe { abi::active_tab_id(state, 9999) }, 0);
    assert_eq!(unsafe { abi::open_tab(state, 9999, c("https://x.example").as_ptr()) }, 0);
    assert!(!unsafe { abi::close_window(state, 9999) });

    unsafe { abi::state_free(state) };
}

// The four optional data files a profile may carry. Present means the
// user's own list replaces the built-in one; absent is the normal case.
#[test]
fn a_profile_can_supply_its_own_blocklist() {
    let f = guard("override");
    let profile = f.dir.join("Weglet").join("Default");
    std::fs::create_dir_all(&profile).unwrap();
    std::fs::write(
        profile.join("blocklist.txt"),
        "# mine\nnot-doubleclick.example\n",
    )
    .unwrap();

    let state = abi::state_new();
    // Replaces rather than extends: someone who supplies a list has a
    // reason, and silently keeping entries they removed would defeat it.
    assert!(unsafe {
        abi::is_url_blocked(
            state,
            c("https://not-doubleclick.example/").as_ptr(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
        )
    });
    assert!(!unsafe { abi::is_host_blocked(state, c("stats.doubleclick.net").as_ptr()) });
    unsafe { abi::state_free(state) };
}

// The whole point of deferring: changing a setting must not write.
#[test]
fn settings_are_marked_dirty_and_written_only_on_flush() {
    let f = guard("flush");
    let state = abi::state_new();
    let settings_file = f.dir.join("Weglet").join("Default").join("settings.toml");

    assert!(!unsafe { abi::settings_dirty(state) });
    assert!(unsafe { abi::set_accent_color(state, c("#3B82F6").as_ptr()) });
    assert_eq!(take(unsafe { abi::accent_color(state) }), "#3B82F6");

    assert!(unsafe { abi::settings_dirty(state) });
    assert!(!settings_file.exists(), "the change must not have touched the disk");

    assert!(unsafe { abi::flush_settings(state) });
    assert!(settings_file.exists());
    assert!(!unsafe { abi::settings_dirty(state) });

    // Nothing pending: a flush is a cheap no-op, so the browser can call
    // it on a timer without thinking about it.
    assert!(unsafe { abi::flush_settings(state) });

    unsafe { abi::state_free(state) };

    // And it really round-trips: a second state reads back the colour.
    let state = abi::state_new();
    assert_eq!(take(unsafe { abi::accent_color(state) }), "#3B82F6");
    unsafe { abi::state_free(state) };
}

// A rejected value must leave neither the setting nor the dirty flag
// changed, or the browser writes the file to save nothing.
#[test]
fn a_rejected_setting_does_not_mark_anything_dirty() {
    let _f = guard("reject");
    let state = abi::state_new();
    let before = take(unsafe { abi::accent_color(state) });
    assert!(!unsafe { abi::set_accent_color(state, c("not-a-colour").as_ptr()) });
    assert_eq!(take(unsafe { abi::accent_color(state) }), before);
    assert!(!unsafe { abi::settings_dirty(state) });
    unsafe { abi::state_free(state) };
}
