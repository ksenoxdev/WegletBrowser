// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-profile/src/session.rs

use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::Error;

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, rename_all = "kebab-case")]
pub struct SessionTab {
    pub entries: Vec<String>,
    pub cursor: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, rename_all = "kebab-case")]
pub struct Session {
    pub tabs: Vec<SessionTab>,
    pub active: usize,
}

// One renderer process per tab on restore, so this is a startup cost as
// much as a memory one. Matches AppState's own ceiling.
const MAX_TABS: usize = 100;

impl Session {
    // A missing file is a first run. A corrupt one is an error the caller
    // can report -- but note the caller is expected to fall back to a
    // fresh session rather than refuse to start: a broken session file
    // must never be the reason the browser will not open.
    pub fn load(path: &Path) -> Result<Self, Error> {
        let text = match std::fs::read_to_string(path) {
            Ok(text) => text,
            Err(source) if source.kind() == std::io::ErrorKind::NotFound => {
                return Ok(Self::default());
            }
            Err(source) => {
                return Err(Error::Read {
                    path: path.to_path_buf(),
                    source,
                })
            }
        };

        let mut session: Self = toml::from_str(&text).map_err(|source| Error::Parse {
            path: path.to_path_buf(),
            source,
        })?;
        session.repair();
        Ok(session)
    }

    pub fn save(&self, path: &Path) -> Result<(), Error> {
        let text = toml::to_string_pretty(self).map_err(|source| Error::Serialise {
            path: path.to_path_buf(),
            source,
        })?;
        crate::atomic::write(path, &text)
    }

    // Makes anything that came off disk safe to index with. Every number
    // here was written by a previous run, or by whatever else touched the
    // file, and an out-of-range one would panic at startup.
    fn repair(&mut self) {
        self.tabs.truncate(MAX_TABS);
        self.tabs.retain(|tab| !tab.entries.is_empty());
        for tab in &mut self.tabs {
            tab.cursor = tab.cursor.min(tab.entries.len() - 1);
        }
        if self.tabs.is_empty() {
            self.active = 0;
        } else {
            self.active = self.active.min(self.tabs.len() - 1);
        }
    }

    // The shape AppState::restore wants. Keeps the two crates from having
    // to know each other's types.
    pub fn to_restore_input(&self) -> (Vec<(Vec<String>, usize)>, usize) {
        let tabs = self
            .tabs
            .iter()
            .map(|tab| (tab.entries.clone(), tab.cursor))
            .collect();
        (tabs, self.active)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn file(name: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("weglet-session-{name}"));
        let _ = std::fs::remove_dir_all(&dir);
        dir.join("session.toml")
    }

    fn tab(entries: &[&str], cursor: usize) -> SessionTab {
        SessionTab {
            entries: entries.iter().map(|s| s.to_string()).collect(),
            cursor,
        }
    }

    #[test]
    fn a_missing_file_is_an_empty_session() {
        assert_eq!(Session::load(&file("missing")).unwrap(), Session::default());
    }

    #[test]
    fn a_session_round_trips_through_disk() {
        let path = file("roundtrip");
        let session = Session {
            tabs: vec![tab(&["https://a.example", "https://a2.example"], 1)],
            active: 0,
        };
        session.save(&path).unwrap();
        assert_eq!(Session::load(&path).unwrap(), session);
    }

    // Every one of these would panic at startup if the number were used
    // as written.
    #[test]
    fn out_of_range_indices_are_repaired_on_load() {
        let path = file("repair");
        Session {
            tabs: vec![tab(&["https://a.example"], 99)],
            active: 42,
        }
        .save(&path)
        .unwrap();

        let session = Session::load(&path).unwrap();
        assert_eq!(session.tabs[0].cursor, 0);
        assert_eq!(session.active, 0);
    }

    // A tab with no history has no URL to open, so it is not a tab.
    #[test]
    fn tabs_with_no_entries_are_dropped() {
        let path = file("empty-tab");
        Session {
            tabs: vec![tab(&[], 0), tab(&["https://a.example"], 0)],
            active: 1,
        }
        .save(&path)
        .unwrap();

        let session = Session::load(&path).unwrap();
        assert_eq!(session.tabs.len(), 1);
        assert_eq!(session.tabs[0].entries, ["https://a.example"]);
        assert_eq!(session.active, 0);
    }

    #[test]
    fn an_oversized_session_is_truncated() {
        let path = file("cap");
        Session {
            tabs: (0..MAX_TABS + 20)
                .map(|i| SessionTab {
                    entries: vec![format!("https://{i}.example")],
                    cursor: 0,
                })
                .collect(),
            active: MAX_TABS + 19,
        }
        .save(&path)
        .unwrap();

        let session = Session::load(&path).unwrap();
        assert_eq!(session.tabs.len(), MAX_TABS);
        assert_eq!(session.active, MAX_TABS - 1);
    }

    #[test]
    fn a_malformed_file_is_an_error() {
        let path = file("malformed");
        crate::atomic::write(&path, "tabs = not-toml = =").unwrap();
        assert!(matches!(Session::load(&path), Err(Error::Parse { .. })));
    }

    #[test]
    fn the_restore_shape_carries_every_tab_and_the_active_index() {
        let session = Session {
            tabs: vec![tab(&["https://a.example"], 0), tab(&["https://b.example"], 0)],
            active: 1,
        };
        let (tabs, active) = session.to_restore_input();
        assert_eq!(tabs.len(), 2);
        assert_eq!(tabs[1].0, ["https://b.example"]);
        assert_eq!(active, 1);
    }
}
