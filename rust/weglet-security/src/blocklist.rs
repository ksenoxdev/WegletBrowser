// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// crates/weglet-security/src/blocklist.rs
//
// Static ad/tracker domains, ported from the Android app's own list.
// Only useful for navigation-time blocking here (see risk.rs's own
// caller) -- real per-resource ad blocking needs raw WebView2 COM
// access wry doesn't expose, unlike native downloads in weglet-window.

use std::collections::HashSet;
use std::sync::OnceLock;

const BLOCKED_HOSTS: &[&str] = &[
    "adnxs.com",
    "ads.microsoft.com",
    "adsrvr.org",
    "amazon-adsystem.com",
    "2mdn.net",
    "adservice.google.com",
    "adservice.google.ru",
    "app-measurement.com",
    "bat.bing.com",
    "clarity.ms",
    "connect.facebook.net",
    "criteo.com",
    "criteo.net",
    "doubleclick.net",
    "google-analytics.com",
    "googlesyndication.com",
    "googletagmanager.com",
    "googletagservices.com",
    "hotjar.com",
    "mc.yandex.ru",
    "metrika.yandex.ru",
    "pixel.facebook.com",
    "quantserve.com",
    "scorecardresearch.com",
    "segment.io",
    "taboola.com",
    "track.adform.net",
    "yandexadexchange.net",
];

// One spelling of a host. Without this "EXAMPLE.com", "example.com.",
// the punycode form and the unicode form of one domain are four
// different strings, and blocking one leaves the other three working.
pub fn canonical_host(host: &str) -> String {
    let trimmed = host.trim().trim_end_matches('.').to_lowercase();
    // An IPv6 literal has no idna form; leave it as written.
    if trimmed.contains(':') {
        return trimmed;
    }
    idna::domain_to_ascii(&trimmed).unwrap_or(trimmed)
}

// The built-in list, canonicalised once at first use rather than on
// every request. This runs for every subresource of every page, so the
// per-call cost is the whole point.
fn blocked_set() -> &'static HashSet<String> {
    static SET: OnceLock<HashSet<String>> = OnceLock::new();
    SET.get_or_init(|| {
        BLOCKED_HOSTS
            .iter()
            .map(|host| canonical_host(host))
            .collect()
    })
}

// Walks up the domain (a.b.example.com -> b.example.com -> ...) so a
// subdomain of a blocked host is caught too, not just an exact match.
pub fn is_blocked_host(host: &str) -> bool {
    let set = blocked_set();
    walk_up(&canonical_host(host), |candidate| set.contains(candidate))
}

pub fn matches_user_blocklist(host: &str, blocked: &[String]) -> bool {
    if blocked.is_empty() {
        return false;
    }
    // The user's list is small and changes at runtime, so it is
    // canonicalised per call rather than cached.
    let blocked: HashSet<String> = blocked.iter().map(|host| canonical_host(host)).collect();
    walk_up(&canonical_host(host), |candidate| {
        blocked.contains(candidate)
    })
}

fn walk_up(host: &str, matches: impl Fn(&str) -> bool) -> bool {
    let mut candidate = host;
    loop {
        if matches(candidate) {
            return true;
        }
        match candidate.find('.') {
            Some(dot) => candidate = &candidate[dot + 1..],
            None => return false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_exact_blocked_host_matches() {
        assert!(is_blocked_host("doubleclick.net"));
    }

    #[test]
    fn a_subdomain_of_a_blocked_host_matches() {
        assert!(is_blocked_host("stats.doubleclick.net"));
    }

    #[test]
    fn an_unrelated_host_does_not_match() {
        assert!(!is_blocked_host("example.com"));
    }

    #[test]
    fn a_host_that_merely_contains_a_blocked_name_does_not_match() {
        // Not a suffix match -- "notdoubleclick.net" != "doubleclick.net".
        assert!(!is_blocked_host("notdoubleclick.net"));
    }

    #[test]
    fn user_blocklist_matches_exact_and_subdomains() {
        let blocked = vec!["evil.example".to_string()];
        assert!(matches_user_blocklist("evil.example", &blocked));
        assert!(matches_user_blocklist("sub.evil.example", &blocked));
        assert!(!matches_user_blocklist("example.com", &blocked));
    }

    #[test]
    fn an_empty_user_blocklist_matches_nothing() {
        assert!(!matches_user_blocklist("evil.example", &[]));
    }

    // One domain, four spellings. Blocking it once has to block all of
    // them, or the setting is theatre.
    #[test]
    fn a_host_matches_however_it_is_spelled() {
        assert!(is_blocked_host("DoubleClick.net"));
        assert!(is_blocked_host("doubleclick.net."));
        assert!(is_blocked_host("STATS.DoubleClick.NET."));

        let blocked = vec!["\u{41f}\u{440}\u{438}\u{43c}\u{435}\u{440}.\u{440}\u{444}".to_string()];
        assert!(matches_user_blocklist("xn--e1afmkfd.xn--p1ai", &blocked));
        assert!(matches_user_blocklist(
            "\u{43F}\u{440}\u{438}\u{43C}\u{435}\u{440}.\u{440}\u{444}",
            &blocked
        ));
        assert!(matches_user_blocklist(
            "sub.xn--e1afmkfd.xn--p1ai",
            &blocked
        ));
    }

    #[test]
    fn canonical_host_leaves_an_ipv6_literal_alone() {
        assert_eq!(canonical_host("[::1]"), "[::1]");
    }
}
