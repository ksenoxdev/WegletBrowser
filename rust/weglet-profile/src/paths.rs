// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-profile/src/paths.rs

use std::path::{Path, PathBuf};

// Where the profile lives. One place that decides, so no two callers can
// disagree about which directory is the profile.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Paths {
    root: PathBuf,
}

impl Paths {
    // Must agree with WegletBrowserContext::DefaultProfilePath on the C++
    // side -- the engine's cookies and this crate's settings belong to the
    // same profile and have to sit under the same directory.
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

    pub fn session_file(&self) -> PathBuf {
        self.root.join("session.toml")
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
