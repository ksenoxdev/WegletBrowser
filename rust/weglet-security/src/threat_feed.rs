// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// OpenPhish's public feed, kept as SHA-256 hashes of URL indicators so a
// browsing URL is checked locally and never sent anywhere itself.
//
// The fetch is the browser process's job -- it already has a network
// stack. This module only ever sees text the browser already downloaded.

use std::collections::HashSet;
use std::sync::{OnceLock, RwLock};

use sha2::{Digest, Sha256};

const MAX_FEED_LINES: usize = 50_000;
const MIN_INDICATORS: usize = 50;

// Wording lives here, not in the browser process: the page renders
// whatever it is handed and has no words of its own, the same way
// blocklist.rs's USER_BLOCK_TITLE/USER_BLOCK_REASON work.
pub const KNOWN_PHISHING_TITLE: &str = "Known phishing site";
pub const KNOWN_PHISHING_REASON: &str =
    "This address matches a known phishing site from a community threat feed.";

// The live indicator set, seeded from the persisted cache at startup and
// replaced whenever a refresh succeeds. A RwLock rather than the
// set-once OnceLock pattern the rest of this crate uses elsewhere: a
// refresh can happen many times in one process, not just once at
// startup.
fn live_hashes() -> &'static RwLock<HashSet<String>> {
    static HASHES: OnceLock<RwLock<HashSet<String>>> = OnceLock::new();
    HASHES.get_or_init(|| RwLock::new(HashSet::new()))
}

// Called once at startup with whatever the profile's cache holds, and
// again after every successful refresh.
pub fn set_live_hashes(hashes: HashSet<String>) {
    *live_hashes().write().unwrap() = hashes;
}

pub fn is_known_phishing_now(url: &str) -> bool {
    is_known_phishing(url, &live_hashes().read().unwrap())
}

pub struct ParsedFeed {
    pub hashes: HashSet<String>,
    pub source_count: usize,
}

// `body` is the feed's raw text, already fetched by the browser process.
// Rejected rather than accepted-but-empty when it looks broken: a feed
// that fails to parse should leave the last good cache in place, not
// silently replace it with nothing.
pub fn parse_feed(body: &str) -> Result<ParsedFeed, String> {
    let mut hashes = HashSet::new();
    let mut source_count = 0usize;
    for line in body.lines().take(MAX_FEED_LINES) {
        let line = line.trim();
        if !line.starts_with("http://") && !line.starts_with("https://") {
            continue;
        }
        let line_indicators = indicators_for_url(line);
        if !line_indicators.is_empty() {
            hashes.extend(line_indicators);
            source_count += 1;
        }
    }

    if hashes.len() < MIN_INDICATORS {
        return Err(format!(
            "threat feed produced too few indicators ({}, expected at least {MIN_INDICATORS})",
            hashes.len()
        ));
    }
    Ok(ParsedFeed {
        hashes,
        source_count,
    })
}

pub fn is_known_phishing(url: &str, hashes: &HashSet<String>) -> bool {
    if hashes.is_empty() {
        return false;
    }
    indicators_for_url(url)
        .iter()
        .any(|indicator| hashes.contains(indicator))
}

// Authority + path, and authority + path + query when there's a query --
// a non-root phishing route usually stays malicious when tracking
// params change, so both the with- and without-query forms are indexed.
fn indicators_for_url(raw_url: &str) -> Vec<String> {
    let Ok(parsed) = url::Url::parse(raw_url) else {
        return Vec::new();
    };
    if parsed.scheme() != "http" && parsed.scheme() != "https" {
        return Vec::new();
    }
    let Some(host) = parsed.host_str() else {
        return Vec::new();
    };
    let authority = match parsed.port() {
        Some(port) if port != 80 && port != 443 => format!("{host}:{port}"),
        _ => host.to_string(),
    };
    let mut path = parsed.path().to_string();
    while path.contains("//") {
        path = path.replace("//", "/");
    }
    if path.len() > 1 {
        path = path.trim_end_matches('/').to_string();
    }
    path.truncate(2048.min(path.len()));
    let query: String = parsed.query().unwrap_or("").chars().take(2048).collect();

    let base = format!("{authority}|{path}");
    let mut plain = Vec::new();
    if query.is_empty() {
        plain.push(base);
    } else {
        plain.push(format!("{base}?{query}"));
        if path != "/" {
            plain.push(base);
        }
    }
    plain.iter().map(|value| sha256(value)).collect()
}

fn sha256(value: &str) -> String {
    let digest = Sha256::digest(value.as_bytes());
    digest.iter().map(|byte| format!("{byte:02x}")).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn feed_line(url: &str) -> String {
        format!("{url}\n")
    }

    #[test]
    fn the_same_url_always_hashes_the_same_way() {
        let a = indicators_for_url("https://evil.example/login");
        let b = indicators_for_url("https://evil.example/login");
        assert_eq!(a, b);
        assert!(!a.is_empty());
    }

    #[test]
    fn a_query_string_produces_both_with_and_without_query_indicators() {
        let indicators = indicators_for_url("https://evil.example/login?id=123");
        assert_eq!(indicators.len(), 2);
    }

    #[test]
    fn the_root_path_with_a_query_only_indexes_the_with_query_form() {
        let indicators = indicators_for_url("https://evil.example/?id=123");
        assert_eq!(indicators.len(), 1);
    }

    #[test]
    fn a_non_http_url_produces_no_indicators() {
        assert!(indicators_for_url("ftp://evil.example/file").is_empty());
    }

    #[test]
    fn is_known_phishing_matches_a_hashed_indicator() {
        let indicators = indicators_for_url("https://evil.example/login");
        let hashes: HashSet<String> = indicators.into_iter().collect();
        assert!(is_known_phishing("https://evil.example/login", &hashes));
        assert!(!is_known_phishing("https://safe.example/", &hashes));
    }

    #[test]
    fn an_empty_hash_set_matches_nothing() {
        assert!(!is_known_phishing("https://evil.example/login", &HashSet::new()));
    }

    #[test]
    fn parsing_a_feed_below_the_indicator_floor_is_rejected() {
        let body = feed_line("https://evil.example/one");
        assert!(parse_feed(&body).is_err());
    }

    #[test]
    fn parsing_a_real_looking_feed_collects_every_indicator() {
        let mut body = String::new();
        for i in 0..(MIN_INDICATORS + 10) {
            body.push_str(&feed_line(&format!("https://evil{i}.example/login")));
        }
        let parsed = parse_feed(&body).unwrap();
        assert!(parsed.hashes.len() >= MIN_INDICATORS);
        assert_eq!(parsed.source_count, MIN_INDICATORS + 10);
    }

    #[test]
    fn non_url_lines_are_skipped_without_counting_as_a_source() {
        let mut body = "# a comment\n\n".to_string();
        for i in 0..MIN_INDICATORS {
            body.push_str(&feed_line(&format!("https://evil{i}.example/login")));
        }
        let parsed = parse_feed(&body).unwrap();
        assert_eq!(parsed.source_count, MIN_INDICATORS);
    }
}
