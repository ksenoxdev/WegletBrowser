// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-core/src/tab.rs

use crate::History;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct TabId(u64);

impl TabId {
    pub fn new(value: u64) -> Self {
        Self(value)
    }

    pub fn value(self) -> u64 {
        self.0
    }
}

// Which window a tab is in.
//
// The C++ side always expected several: it counts windows, quits when the
// last one closes, and looks a window up from a WebContents. The model
// had one flat list of tabs and one active tab, so a second window would
// have shown the same tabs and fought over which was in front. Ids rather
// than an index because a window in the middle can close.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct WindowId(u64);

impl WindowId {
    pub fn new(value: u64) -> Self {
        Self(value)
    }

    pub fn value(self) -> u64 {
        self.0
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tab {
    pub id: TabId,
    // The window it belongs to. A tab is never in two, and never in none:
    // AppState creates every tab against a window that exists.
    pub window: WindowId,
    pub history: History,
    // What the page called itself. Empty until the engine reports one, so
    // `label` falls back to the host rather than showing nothing.
    pub title: String,
    pub loading: bool,
}

impl Tab {
    pub fn new(id: TabId, window: WindowId, url: String) -> Self {
        Self {
            id,
            window,
            history: History::new(url),
            title: String::new(),
            loading: false,
        }
    }

    // Reads through to the history cursor. There is no second copy of the
    // current URL on the tab, so the address bar and the loaded page
    // cannot disagree.
    pub fn url(&self) -> &str {
        self.history.current()
    }

    pub fn navigate(&mut self, url: String) {
        self.history.navigate(url);
        // The old title belongs to the old page. Keeping it would leave
        // the tab strip showing the previous page's name for as long as
        // the new one takes to load.
        self.title.clear();
    }

    // What to show in the tab strip. Our own pages get a name; everything
    // else gets its title, and its host until the title arrives.
    pub fn label(&self) -> &str {
        match self.url() {
            crate::BLANK_TAB => "New Tab",
            crate::SETTINGS_ADDRESS => "Settings",
            crate::HISTORY_ADDRESS => "History",
            crate::BOOKMARKS_ADDRESS => "Bookmarks",
            crate::TERMS_ADDRESS => "Welcome",
            url => {
                if self.title.is_empty() {
                    weglet_url::display_host(url)
                } else {
                    &self.title
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tab(url: &str) -> Tab {
        Tab::new(TabId::new(1), WindowId::new(0), url.to_string())
    }

    #[test]
    fn a_tabs_url_comes_from_its_history() {
        let mut tab = tab("https://a.example");
        assert_eq!(tab.url(), "https://a.example");
        tab.navigate("https://b.example".into());
        assert_eq!(tab.url(), "https://b.example");
        assert_eq!(tab.history.current(), tab.url());
    }

    // Every one of our pages needs a name here. A missing arm shows the
    // raw host, which is how "bookmarks" ended up in the tab strip before.
    #[test]
    fn every_internal_address_has_a_readable_name() {
        for (url, expected) in [
            (crate::BLANK_TAB, "New Tab"),
            (crate::SETTINGS_ADDRESS, "Settings"),
            (crate::HISTORY_ADDRESS, "History"),
            (crate::BOOKMARKS_ADDRESS, "Bookmarks"),
            (crate::TERMS_ADDRESS, "Welcome"),
        ] {
            assert_eq!(tab(url).label(), expected, "{url}");
        }
    }

    #[test]
    fn a_web_page_shows_its_host_until_a_title_arrives() {
        let mut tab = tab("https://sub.example.com/deep/path");
        assert_eq!(tab.label(), "sub.example.com");
        tab.title = "Real Title".into();
        assert_eq!(tab.label(), "Real Title");
    }

    #[test]
    fn navigating_drops_the_previous_pages_title() {
        let mut tab = tab("https://a.example");
        tab.title = "Page A".into();
        tab.navigate("https://b.example".into());
        assert_eq!(tab.label(), "b.example");
    }
}
