// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-profile/src/settings.rs

use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::Error;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum SearchEngine {
    DuckDuckGo,
    Google,
    Bing,
}

impl SearchEngine {
    // DuckDuckGo, and not because anybody paid for it. A browser that
    // says it makes no request the user did not ask for cannot ship with
    // a default that reports every search to an advertising company.
    pub const DEFAULT: Self = Self::DuckDuckGo;

    pub fn query_url(self, query: &str) -> String {
        let escaped = percent_encode_query(query);
        match self {
            Self::DuckDuckGo => format!("https://duckduckgo.com/?q={escaped}"),
            Self::Google => format!("https://www.google.com/search?q={escaped}"),
            Self::Bing => format!("https://www.bing.com/search?q={escaped}"),
        }
    }
}

// Written out rather than pulling in a crate: this is the only place the
// browser percent-encodes anything, and the rule is small enough to read.
// Unreserved set per RFC 3986, everything else escaped -- including "+",
// which a receiver would otherwise decode as a space.
fn percent_encode_query(query: &str) -> String {
    let mut out = String::with_capacity(query.len());
    for byte in query.as_bytes() {
        match byte {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'.' | b'_' | b'~' => {
                out.push(*byte as char);
            }
            b' ' => out.push_str("%20"),
            other => out.push_str(&format!("%{other:02X}")),
        }
    }
    out
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, rename_all = "kebab-case")]
pub struct Settings {
    pub search_engine: SearchEngine,
    pub restore_session: bool,
    pub blocked_hosts: Vec<String>,
    // Until this is true the only page that opens is the terms screen.
    pub terms_accepted: bool,
}

// The block list is walked for every request the blocker sees, so an
// unbounded one from a hand-edited file is a performance cliff rather
// than a feature.
const MAX_BLOCKED_HOSTS: usize = 10_000;

impl Default for Settings {
    fn default() -> Self {
        Self {
            search_engine: SearchEngine::DEFAULT,
            restore_session: true,
            blocked_hosts: Vec::new(),
            terms_accepted: false,
        }
    }
}

impl Settings {
    // A missing file is not an error -- that is a first run. Anything else
    // is returned so the caller can say so out loud instead of silently
    // substituting defaults and overwriting the user's real settings on
    // the next save.
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

        let mut settings: Self = toml::from_str(&text).map_err(|source| Error::Parse {
            path: path.to_path_buf(),
            source,
        })?;
        settings.clamp();
        Ok(settings)
    }

    pub fn save(&self, path: &Path) -> Result<(), Error> {
        let text = toml::to_string_pretty(self).map_err(|source| Error::Serialise {
            path: path.to_path_buf(),
            source,
        })?;
        crate::atomic::write(path, &text)
    }

    // Applied on load, not on save, so an oversized file is only rewritten
    // smaller once the user actually changes something.
    fn clamp(&mut self) {
        if self.blocked_hosts.len() > MAX_BLOCKED_HOSTS {
            self.blocked_hosts.truncate(MAX_BLOCKED_HOSTS);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn file(name: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("weglet-settings-{name}"));
        let _ = std::fs::remove_dir_all(&dir);
        dir.join("settings.toml")
    }

    #[test]
    fn a_missing_file_is_a_first_run_not_an_error() {
        assert_eq!(Settings::load(&file("missing")).unwrap(), Settings::default());
    }

    #[test]
    fn settings_round_trip_through_disk() {
        let path = file("roundtrip");
        let settings = Settings {
            search_engine: SearchEngine::Bing,
            restore_session: false,
            blocked_hosts: vec!["ads.example".into()],
            terms_accepted: true,
        };
        settings.save(&path).unwrap();
        assert_eq!(Settings::load(&path).unwrap(), settings);
    }

    // An unknown key is a file written by a newer version. Losing the
    // whole profile over it would be worse than ignoring it.
    #[test]
    fn an_unknown_key_is_ignored() {
        let path = file("unknown");
        crate::atomic::write(&path, "search-engine = \"bing\"\nfuture-option = 42\n").unwrap();
        assert_eq!(Settings::load(&path).unwrap().search_engine, SearchEngine::Bing);
    }

    #[test]
    fn a_partial_file_keeps_the_defaults_for_what_is_missing() {
        let path = file("partial");
        crate::atomic::write(&path, "terms-accepted = true\n").unwrap();
        let settings = Settings::load(&path).unwrap();
        assert!(settings.terms_accepted);
        assert_eq!(settings.search_engine, SearchEngine::DEFAULT);
        assert!(settings.restore_session);
    }

    // Loud, not silent: the caller has to be able to tell the user rather
    // than quietly resetting them.
    #[test]
    fn a_malformed_file_is_an_error() {
        let path = file("malformed");
        crate::atomic::write(&path, "this is not toml = = =").unwrap();
        assert!(matches!(
            Settings::load(&path),
            Err(Error::Parse { .. })
        ));
    }

    #[test]
    fn an_oversized_block_list_is_clamped_on_load() {
        let path = file("clamp");
        let settings = Settings {
            blocked_hosts: (0..MAX_BLOCKED_HOSTS + 500)
                .map(|i| format!("{i}.example"))
                .collect(),
            ..Settings::default()
        };
        settings.save(&path).unwrap();
        assert_eq!(
            Settings::load(&path).unwrap().blocked_hosts.len(),
            MAX_BLOCKED_HOSTS
        );
    }

    #[test]
    fn the_default_engine_is_duckduckgo() {
        assert_eq!(Settings::default().search_engine, SearchEngine::DuckDuckGo);
    }

    #[test]
    fn a_query_is_percent_encoded() {
        assert_eq!(
            SearchEngine::DuckDuckGo.query_url("rust browser"),
            "https://duckduckgo.com/?q=rust%20browser"
        );
        // Every one of these would change the meaning of the URL if it
        // went through unescaped.
        assert_eq!(
            SearchEngine::Google.query_url("a&b=c#d?e"),
            "https://www.google.com/search?q=a%26b%3Dc%23d%3Fe"
        );
        // "+" must not survive: a receiver would read it as a space.
        assert_eq!(
            SearchEngine::Bing.query_url("c++"),
            "https://www.bing.com/search?q=c%2B%2B"
        );
    }

    #[test]
    fn a_non_ascii_query_is_encoded_as_utf8_bytes() {
        assert_eq!(
            SearchEngine::DuckDuckGo.query_url("\u{43F}\u{440}\u{438}\u{432}\u{435}\u{442}"),
            "https://duckduckgo.com/?q=%D0%BF%D1%80%D0%B8%D0%B2%D0%B5%D1%82"
        );
    }
}
