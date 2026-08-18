// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-core/src/lib.rs
//
// The browser's model: what tabs exist, which one is in front, where
// each has been. No IO, no engine, no platform -- so all of it is
// testable without starting a browser, and that is the point.

mod history;
mod omnibox;
mod state;
mod tab;

pub use history::History;
pub use omnibox::{parse as parse_omnibox, Action as OmniboxAction};
pub use state::{AppState, RestoredTab, RestoredWindow, Window};
pub use tab::{Tab, TabId, WindowId};

// Weglet's own addresses and is_internal_address(), generated from
// weglet/ui/contract.json by rust/build_rust.py before every cargo build.
// Not written here by hand: the C++ side generates the same five addresses
// from the same file, and they went out of step once when they were two
// separate lists -- three addresses existed here with nothing on the C++
// side to resolve them to a real page.
include!("generated_addresses.rs");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn our_own_addresses_are_recognised() {
        for url in [
            BLANK_TAB,
            SETTINGS_ADDRESS,
            HISTORY_ADDRESS,
            BOOKMARKS_ADDRESS,
            TERMS_ADDRESS,
        ] {
            assert!(is_internal_address(url), "{url}");
        }
    }

    // A prefix or suffix test here would be a privilege hole: these
    // addresses decide which pages get profile data pushed into them.
    #[test]
    fn a_lookalike_address_is_not_one_of_ours() {
        for url in [
            "weglet://settings.evil.example",
            "weglet://settingsx",
            "https://weglet.example/settings",
            "https://evil.example/weglet://settings",
            "weglet://",
            "",
        ] {
            assert!(!is_internal_address(url), "{url}");
        }
    }
}
