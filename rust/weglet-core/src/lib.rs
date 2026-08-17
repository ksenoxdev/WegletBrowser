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
pub use state::AppState;
pub use tab::{Tab, TabId};

// Weglet's own pages. Addresses the user can see and type; the engine
// never receives these strings, the C++ side maps them to real URLs.
pub const BLANK_TAB: &str = "about:blank";
pub const SETTINGS_ADDRESS: &str = "weglet://settings";
pub const HISTORY_ADDRESS: &str = "weglet://history";
pub const BOOKMARKS_ADDRESS: &str = "weglet://bookmarks";
pub const TERMS_ADDRESS: &str = "weglet://terms";

// A page of ours, in the form the user types it. Deliberately exact
// matches: a prefix test would make "weglet://settings.evil.example" one
// of our pages.
pub fn is_internal_address(url: &str) -> bool {
    matches!(
        url,
        BLANK_TAB | SETTINGS_ADDRESS | HISTORY_ADDRESS | BOOKMARKS_ADDRESS | TERMS_ADDRESS
    )
}

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
