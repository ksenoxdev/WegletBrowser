// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The profile directory and the files inside it.

use std::path::{Path, PathBuf};

// Where the profile lives. One place decides, so no two callers can
// disagree.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Paths {
    root: PathBuf,
}

impl Paths {
    // Must agree with WegletBrowserContext::DefaultProfilePath: the
    // engine's cookies and this crate's settings share a directory.
    pub fn discover() -> Result<Self, crate::Error> {
        let root = platform_root().ok_or(crate::Error::NoProfileDirectory)?;
        Ok(Self {
            root: root.join("Weglet").join("Default"),
        })
    }

    // For tests and for a future --user-data-dir.
    pub fn at(root: impl Into<PathBuf>) -> Self {
        Self { root: root.into() }
    }

    pub fn root(&self) -> &Path {
        &self.root
    }

    pub fn settings_file(&self) -> PathBuf {
        self.root.join("settings.toml")
    }

    // Optional data files. Present means the user wants their own list;
    // absent means the compiled-in table stands. Nothing is ever written
    // to them.
    pub fn blocklist_file(&self) -> PathBuf {
        self.root.join("blocklist.txt")
    }

    pub fn brands_file(&self) -> PathBuf {
        self.root.join("brands.toml")
    }

    pub fn sensitive_words_file(&self) -> PathBuf {
        self.root.join("sensitive_words.txt")
    }

    pub fn engines_file(&self) -> PathBuf {
        self.root.join("engines.toml")
    }

    pub fn session_file(&self) -> PathBuf {
        self.root.join("session.toml")
    }

    pub fn bookmarks_file(&self) -> PathBuf {
        self.root.join("bookmarks.toml")
    }

    pub fn history_file(&self) -> PathBuf {
        self.root.join("history.toml")
    }

    pub fn downloads_file(&self) -> PathBuf {
        self.root.join("downloads.toml")
    }

    pub fn threat_feed_file(&self) -> PathBuf {
        self.root.join("threat-feed.toml")
    }
}

#[cfg(target_os = "windows")]
fn platform_root() -> Option<PathBuf> {
    std::env::var_os("LOCALAPPDATA").map(PathBuf::from)
}

#[cfg(not(target_os = "windows"))]
fn platform_root() -> Option<PathBuf> {
    if let Some(dir) = std::env::var_os("XDG_DATA_HOME") {
        return Some(PathBuf::from(dir));
    }
    std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".local").join("share"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_file_sits_under_the_root() {
        let paths = Paths::at("/tmp/profile");
        for file in [paths.settings_file(), paths.session_file()] {
            assert!(file.starts_with(paths.root()), "{file:?}");
        }
    }

    #[test]
    fn the_file_names_are_stable() {
        let paths = Paths::at("/tmp/profile");
        assert!(paths.settings_file().ends_with("settings.toml"));
        assert!(paths.session_file().ends_with("session.toml"));
    }
}
