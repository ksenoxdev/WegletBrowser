// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// A profile's own blocklist.txt replaces the built-in list for the rest
// of the process -- see weglet-security/src/blocklist.rs's OVERRIDE. In
// the real browser that is exactly right: one profile, loaded once, for
// the life of the process. In a test binary it is not, so this test gets
// a file of its own: each file directly under tests/ is a separate
// process, so its permanent, one-time-only override cannot bleed into
// boundary.rs's test that relies on the built-in list.

mod common;
use common::{abi, c, guard};

// The blocklist.txt a profile may carry. Present means the user's own
// list replaces the built-in one, not adds to it.
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
    // Replaces rather than extends: the user removed the built-in entries
    // for a reason, and silently keeping entries they removed would
    // defeat it.
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
