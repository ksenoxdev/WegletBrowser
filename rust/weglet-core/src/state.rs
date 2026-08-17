// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-core/src/state.rs

use crate::{Tab, TabId, BLANK_TAB};

// Which tabs exist and which one is in front. Owns nothing platform-
// specific, so every rule below is testable without a browser.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AppState {
    tabs: Vec<Tab>,
    active: TabId,
    next_id: u64,
}

// A page can call window.open in a loop. Without a ceiling that is
// unbounded memory and one renderer process per tab.
const MAX_TABS: usize = 100;

impl AppState {
    // Always starts with one tab: a browser with zero tabs has no valid
    // representation on screen, and every caller would have to handle it.
    pub fn new() -> Self {
        let id = TabId::new(0);
        Self {
            tabs: vec![Tab::new(id, BLANK_TAB.to_string())],
            active: id,
            next_id: 1,
        }
    }

    // Rebuilds from a restored session. Takes (url, history, cursor)
    // triples rather than Tabs so the profile crate does not have to know
    // how a Tab is put together.
    pub fn restore(tabs: Vec<(Vec<String>, usize)>, active_index: usize) -> Self {
        if tabs.is_empty() {
            return Self::new();
        }

        let mut restored = Vec::new();
        for (index, (entries, cursor)) in tabs.into_iter().take(MAX_TABS).enumerate() {
            let id = TabId::new(index as u64);
            let mut tab = Tab::new(id, BLANK_TAB.to_string());
            tab.history = crate::History::from_entries(entries, cursor);
            restored.push(tab);
        }

        // Clamped, not trusted: the index comes off disk.
        let active_index = active_index.min(restored.len() - 1);
        let active = restored[active_index].id;
        let next_id = restored.len() as u64;

        Self {
            tabs: restored,
            active,
            next_id,
        }
    }

    pub fn tabs(&self) -> &[Tab] {
        &self.tabs
    }

    pub fn active_id(&self) -> TabId {
        self.active
    }

    pub fn active_tab(&self) -> Option<&Tab> {
        self.tab(self.active)
    }

    pub fn active_tab_mut(&mut self) -> Option<&mut Tab> {
        let active = self.active;
        self.tab_mut(active)
    }

    pub fn tab(&self, id: TabId) -> Option<&Tab> {
        self.tabs.iter().find(|tab| tab.id == id)
    }

    pub fn tab_mut(&mut self, id: TabId) -> Option<&mut Tab> {
        self.tabs.iter_mut().find(|tab| tab.id == id)
    }

    pub fn index_of(&self, id: TabId) -> Option<usize> {
        self.tabs.iter().position(|tab| tab.id == id)
    }

    // Returns None when the ceiling is reached rather than silently doing
    // nothing, so the caller can say why.
    pub fn open_tab(&mut self, url: &str) -> Option<TabId> {
        if self.tabs.len() >= MAX_TABS {
            return None;
        }
        let id = TabId::new(self.next_id);
        self.next_id += 1;
        self.tabs.push(Tab::new(id, url.to_string()));
        self.active = id;
        Some(id)
    }

    // True if the tab existed. Closing the last one leaves a fresh blank
    // tab instead of an empty window.
    pub fn close_tab(&mut self, id: TabId) -> bool {
        let Some(index) = self.index_of(id) else {
            return false;
        };
        self.tabs.remove(index);

        if self.tabs.is_empty() {
            let id = TabId::new(self.next_id);
            self.next_id += 1;
            self.tabs.push(Tab::new(id, BLANK_TAB.to_string()));
            self.active = id;
            return true;
        }

        if self.active == id {
            // The one that took its place, or the new last one if it was
            // the rightmost tab. Same as every other browser.
            let next = index.min(self.tabs.len() - 1);
            self.active = self.tabs[next].id;
        }
        true
    }

    pub fn activate(&mut self, id: TabId) -> bool {
        if self.tab(id).is_none() {
            return false;
        }
        self.active = id;
        true
    }

    // Ctrl+Tab. Wraps at both ends.
    pub fn cycle(&mut self, forward: bool) {
        if self.tabs.len() < 2 {
            return;
        }
        let Some(index) = self.index_of(self.active) else {
            return;
        };
        let last = self.tabs.len() - 1;
        let next = if forward {
            if index == last { 0 } else { index + 1 }
        } else if index == 0 {
            last
        } else {
            index - 1
        };
        self.active = self.tabs[next].id;
    }

    // Ctrl+1..9, one-based as on the keyboard. 9 means "the last tab",
    // which is what browsers do.
    pub fn activate_by_position(&mut self, position: usize) -> bool {
        if position == 0 {
            return false;
        }
        let index = if position >= 9 {
            self.tabs.len() - 1
        } else {
            position - 1
        };
        match self.tabs.get(index) {
            Some(tab) => {
                self.active = tab.id;
                true
            }
            None => false,
        }
    }

    // `target` is a plain Vec insertion index, not "the tab to swap with".
    pub fn reorder(&mut self, id: TabId, target: usize) -> bool {
        let Some(from) = self.index_of(id) else {
            return false;
        };
        let target = target.min(self.tabs.len() - 1);
        if from == target {
            return false;
        }
        let tab = self.tabs.remove(from);
        self.tabs.insert(target, tab);
        true
    }
}

impl Default for AppState {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn urls(state: &AppState) -> Vec<&str> {
        state.tabs().iter().map(|tab| tab.url()).collect()
    }

    #[test]
    fn a_new_state_has_exactly_one_blank_active_tab() {
        let state = AppState::new();
        assert_eq!(state.tabs().len(), 1);
        assert_eq!(state.active_tab().unwrap().url(), BLANK_TAB);
    }

    #[test]
    fn opening_a_tab_makes_it_active() {
        let mut state = AppState::new();
        let id = state.open_tab("https://a.example").unwrap();
        assert_eq!(state.active_id(), id);
        assert_eq!(state.tabs().len(), 2);
    }

    #[test]
    fn ids_are_never_reused_after_a_close() {
        let mut state = AppState::new();
        let first = state.open_tab("https://a.example").unwrap();
        state.close_tab(first);
        let second = state.open_tab("https://b.example").unwrap();
        assert_ne!(first, second);
    }

    // Closing the last tab must leave something on screen.
    #[test]
    fn closing_the_only_tab_leaves_a_fresh_blank_one() {
        let mut state = AppState::new();
        let only = state.active_id();
        assert!(state.close_tab(only));
        assert_eq!(state.tabs().len(), 1);
        assert_eq!(state.active_tab().unwrap().url(), BLANK_TAB);
        assert_ne!(state.active_id(), only);
    }

    #[test]
    fn closing_the_active_tab_activates_the_one_that_took_its_place() {
        let mut state = AppState::new();
        let a = state.active_id();
        let b = state.open_tab("https://b.example").unwrap();
        let c = state.open_tab("https://c.example").unwrap();
        state.activate(b);
        state.close_tab(b);
        assert_eq!(state.active_id(), c);
        assert_eq!(state.tabs().len(), 2);
        assert!(state.tab(a).is_some());
    }

    #[test]
    fn closing_the_rightmost_tab_activates_the_new_rightmost() {
        let mut state = AppState::new();
        let a = state.active_id();
        let b = state.open_tab("https://b.example").unwrap();
        state.close_tab(b);
        assert_eq!(state.active_id(), a);
    }

    #[test]
    fn closing_an_inactive_tab_leaves_the_active_one_alone() {
        let mut state = AppState::new();
        let a = state.active_id();
        let b = state.open_tab("https://b.example").unwrap();
        state.activate(a);
        state.close_tab(b);
        assert_eq!(state.active_id(), a);
    }

    #[test]
    fn closing_an_unknown_tab_reports_failure() {
        let mut state = AppState::new();
        assert!(!state.close_tab(TabId::new(999)));
    }

    #[test]
    fn cycling_wraps_at_both_ends() {
        let mut state = AppState::new();
        let a = state.active_id();
        let b = state.open_tab("https://b.example").unwrap();
        state.activate(a);
        state.cycle(false);
        assert_eq!(state.active_id(), b);
        state.cycle(true);
        assert_eq!(state.active_id(), a);
    }

    #[test]
    fn cycling_a_single_tab_does_nothing() {
        let mut state = AppState::new();
        let only = state.active_id();
        state.cycle(true);
        assert_eq!(state.active_id(), only);
    }

    #[test]
    fn position_nine_means_the_last_tab_however_many_there_are() {
        let mut state = AppState::new();
        for i in 0..4 {
            state.open_tab(&format!("https://{i}.example"));
        }
        let last = state.tabs().last().unwrap().id;
        assert!(state.activate_by_position(9));
        assert_eq!(state.active_id(), last);
    }

    #[test]
    fn an_out_of_range_position_is_rejected() {
        let mut state = AppState::new();
        assert!(!state.activate_by_position(0));
        assert!(!state.activate_by_position(5));
    }

    #[test]
    fn reordering_moves_a_tab_to_an_insertion_index() {
        let mut state = AppState::new();
        state.open_tab("https://b.example");
        let c = state.open_tab("https://c.example").unwrap();
        assert!(state.reorder(c, 0));
        assert_eq!(
            urls(&state),
            ["https://c.example", BLANK_TAB, "https://b.example"]
        );
    }

    #[test]
    fn reordering_to_the_same_place_reports_no_change() {
        let mut state = AppState::new();
        let a = state.active_id();
        assert!(!state.reorder(a, 0));
    }

    #[test]
    fn the_tab_count_is_bounded() {
        let mut state = AppState::new();
        while state.open_tab("https://x.example").is_some() {}
        assert_eq!(state.tabs().len(), MAX_TABS);
        assert!(state.open_tab("https://y.example").is_none());
    }

    #[test]
    fn a_restored_session_keeps_each_tabs_position_in_its_history() {
        let state = AppState::restore(
            vec![
                (vec!["https://a.example".into(), "https://a2.example".into()], 0),
                (vec!["https://b.example".into()], 0),
            ],
            1,
        );
        assert_eq!(state.tabs().len(), 2);
        assert_eq!(state.tabs()[0].url(), "https://a.example");
        assert!(state.tabs()[0].history.can_go_forward());
        assert_eq!(state.active_tab().unwrap().url(), "https://b.example");
    }

    #[test]
    fn an_empty_restored_session_becomes_a_fresh_state() {
        let state = AppState::restore(Vec::new(), 0);
        assert_eq!(state.tabs().len(), 1);
        assert_eq!(state.active_tab().unwrap().url(), BLANK_TAB);
    }

    #[test]
    fn an_out_of_range_active_index_is_clamped() {
        let state = AppState::restore(vec![(vec!["https://a.example".into()], 0)], 99);
        assert_eq!(state.active_tab().unwrap().url(), "https://a.example");
    }

    #[test]
    fn a_restored_session_larger_than_the_cap_is_truncated() {
        let tabs: Vec<(Vec<String>, usize)> = (0..MAX_TABS + 20)
            .map(|i| (vec![format!("https://{i}.example")], 0))
            .collect();
        let state = AppState::restore(tabs, 0);
        assert_eq!(state.tabs().len(), MAX_TABS);
    }
}
