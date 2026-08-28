// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Back/forward history for one tab. The cursor is the only record of
// where the tab is; the tab keeps no second copy of the URL.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct History {
    entries: Vec<String>,
    cursor: usize,
}

// Bounded: every entry is a URL held for the life of the tab, and a page
// can push entries in a loop.
const MAX_ENTRIES: usize = 256;

impl History {
    pub fn new(url: String) -> Self {
        Self {
            entries: vec![url],
            cursor: 0,
        }
    }

    // Rebuilds from a restored session. The cursor is clamped, not
    // trusted: it comes off disk, and out of range would panic.
    pub fn from_entries(entries: Vec<String>, cursor: usize) -> Self {
        let mut history = Self {
            entries,
            cursor: 0,
        };
        if history.entries.is_empty() {
            history.entries.push(crate::BLANK_TAB.to_string());
        }
        history.trim();
        history.cursor = cursor.min(history.entries.len() - 1);
        history
    }

    pub fn current(&self) -> &str {
        // Every path that touches entries keeps at least one.
        &self.entries[self.cursor]
    }

    pub fn entries(&self) -> &[String] {
        &self.entries
    }

    pub fn cursor(&self) -> usize {
        self.cursor
    }

    pub fn can_go_back(&self) -> bool {
        self.cursor > 0
    }

    pub fn can_go_forward(&self) -> bool {
        self.cursor + 1 < self.entries.len()
    }

    // A new navigation drops anything ahead of the cursor.
    pub fn navigate(&mut self, url: String) {
        if self.entries[self.cursor] == url {
            return;
        }
        self.entries.truncate(self.cursor + 1);
        self.entries.push(url);
        self.trim();
        self.cursor = self.entries.len() - 1;
    }

    // Same page, new URL: a redirect or history.replaceState. Adds no
    // entry.
    pub fn replace_current(&mut self, url: String) {
        self.entries[self.cursor] = url;
    }

    pub fn go_back(&mut self) -> Option<&str> {
        if !self.can_go_back() {
            return None;
        }
        self.cursor -= 1;
        Some(self.current())
    }

    pub fn go_forward(&mut self) -> Option<&str> {
        if !self.can_go_forward() {
            return None;
        }
        self.cursor += 1;
        Some(self.current())
    }

    // Drops the oldest entries and moves the cursor with them, so the
    // current page stays current.
    fn trim(&mut self) {
        if self.entries.len() <= MAX_ENTRIES {
            return;
        }
        let excess = self.entries.len() - MAX_ENTRIES;
        self.entries.drain(..excess);
        self.cursor = self.cursor.saturating_sub(excess);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn at(urls: &[&str], cursor: usize) -> History {
        History::from_entries(urls.iter().map(|s| s.to_string()).collect(), cursor)
    }

    #[test]
    fn a_new_history_sits_on_its_only_entry() {
        let history = History::new("https://example.com".into());
        assert_eq!(history.current(), "https://example.com");
        assert!(!history.can_go_back());
        assert!(!history.can_go_forward());
    }

    #[test]
    fn navigating_moves_forward_and_enables_back() {
        let mut history = History::new("https://a.example".into());
        history.navigate("https://b.example".into());
        assert_eq!(history.current(), "https://b.example");
        assert!(history.can_go_back());
        assert!(!history.can_go_forward());
    }

    #[test]
    fn back_and_forward_walk_the_entries() {
        let mut history = at(&["https://a.example", "https://b.example"], 1);
        assert_eq!(history.go_back(), Some("https://a.example"));
        assert_eq!(history.go_forward(), Some("https://b.example"));
        assert_eq!(history.go_forward(), None);
    }

    #[test]
    fn navigating_after_going_back_drops_the_forward_branch() {
        let mut history = at(&["https://a.example", "https://b.example"], 0);
        history.navigate("https://c.example".into());
        assert_eq!(history.entries(), ["https://a.example", "https://c.example"]);
        assert!(!history.can_go_forward());
    }

    #[test]
    fn navigating_to_the_same_url_changes_nothing() {
        let mut history = History::new("https://a.example".into());
        history.navigate("https://a.example".into());
        assert_eq!(history.entries().len(), 1);
    }

    // A redirect must not add an entry, or Back lands on the page that
    // redirected.
    #[test]
    fn replacing_the_current_entry_does_not_grow_the_history() {
        let mut history = History::new("https://a.example".into());
        history.replace_current("https://a.example/final".into());
        assert_eq!(history.entries(), ["https://a.example/final"]);
        assert!(!history.can_go_back());
    }

    #[test]
    fn an_out_of_range_cursor_is_clamped_rather_than_trusted() {
        let history = at(&["https://a.example", "https://b.example"], 99);
        assert_eq!(history.current(), "https://b.example");
    }

    #[test]
    fn an_empty_restored_history_becomes_a_blank_tab() {
        let history = History::from_entries(Vec::new(), 0);
        assert_eq!(history.current(), crate::BLANK_TAB);
    }

    #[test]
    fn the_entry_count_is_bounded() {
        let mut history = History::new("https://0.example".into());
        for i in 1..MAX_ENTRIES + 50 {
            history.navigate(format!("https://{i}.example"));
        }
        assert_eq!(history.entries().len(), MAX_ENTRIES);
        // Still on the page it last navigated to.
        assert_eq!(
            history.current(),
            format!("https://{}.example", MAX_ENTRIES + 49)
        );
    }

    #[test]
    fn a_restored_history_longer_than_the_cap_is_trimmed() {
        let urls: Vec<String> = (0..MAX_ENTRIES + 10)
            .map(|i| format!("https://{i}.example"))
            .collect();
        let history = History::from_entries(urls, MAX_ENTRIES + 9);
        assert_eq!(history.entries().len(), MAX_ENTRIES);
        assert_eq!(
            history.current(),
            format!("https://{}.example", MAX_ENTRIES + 9)
        );
    }
}
