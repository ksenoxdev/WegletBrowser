// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Ad and tracker hosts, built-in and the user's own. Asked at navigation
// time; CreateURLLoaderThrottles is the hook for per-resource blocking
// and nothing uses it yet. See docs/security.md.

use std::collections::HashSet;
use std::sync::OnceLock;

// The built-in list, as data. One host per line -- see data/blocklist.txt.
// include_str! rather than a file read at startup: Weglet ships one
// executable and nothing beside it, and set_blocklist_override can
// replace the table at runtime.
const BUILT_IN_BLOCKLIST: &str = include_str!("../data/blocklist.txt");

// A list loaded from the profile, if there is one. Replaces the built-in
// one rather than adding to it.
static OVERRIDE: OnceLock<HashSet<String>> = OnceLock::new();

// One host per line, '#' starts a comment.
pub fn parse_host_list(text: &str) -> HashSet<String> {
    text.lines()
        .map(|line| line.split('#').next().unwrap_or("").trim())
        .filter(|line| !line.is_empty())
        .map(canonical_host)
        .collect()
}

// Installs a list read from the profile. Ignored if called twice: the
// answer changing under an open page is worse than a stale list.
pub fn set_blocklist_override(text: &str) {
    let _ = OVERRIDE.set(parse_host_list(text));
}


// One spelling of a host. Without it "EXAMPLE.com", "example.com.", the
// punycode form and the unicode form are four different strings.
pub fn canonical_host(host: &str) -> String {
    let trimmed = host.trim().trim_end_matches('.').to_lowercase();
    // An IPv6 literal has no idna form; leave it as written.
    if trimmed.contains(':') {
        return trimmed;
    }
    idna::domain_to_ascii(&trimmed).unwrap_or(trimmed)
}

// Canonicalised once at first use: this runs for every subresource of
// every page.
fn blocked_set() -> &'static HashSet<String> {
    if let Some(overridden) = OVERRIDE.get() {
        return overridden;
    }
    static SET: OnceLock<HashSet<String>> = OnceLock::new();
    SET.get_or_init(|| parse_host_list(BUILT_IN_BLOCKLIST))
}

// Walks up the domain (a.b.example.com -> b.example.com -> ...), so a
// subdomain of a blocked host is caught too.
pub fn is_blocked_host(host: &str) -> bool {
    let set = blocked_set();
    walk_up(&canonical_host(host), |candidate| set.contains(candidate))
}

// What the notice says when the user's own list stopped a navigation.
// Here rather than in the browser process because the page renders
// whatever title and reason it is handed and has no wording of its own.
pub const USER_BLOCK_TITLE: &str = "Site blocked";
pub const USER_BLOCK_REASON: &str =
    "You blocked this site. Remove it from the block list in settings to \
     visit it again.";

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
    fn the_built_in_list_parses_and_is_canonical() {
        let hosts = parse_host_list(BUILT_IN_BLOCKLIST);
        assert!(!hosts.is_empty());
        for host in &hosts {
            assert_eq!(host, &canonical_host(host));
            assert!(!host.starts_with('#'));
            assert!(!host.contains(' '));
        }
    }

    #[test]
    fn comments_and_blank_lines_are_ignored() {
        let hosts = parse_host_list(
            "# a comment\n\n  EXAMPLE.com.  \nads.example # trailing\n",
        );
        assert_eq!(hosts.len(), 2);
        assert!(hosts.contains("example.com"));
        assert!(hosts.contains("ads.example"));
    }

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
        // Not a suffix match: "notdoubleclick.net" != "doubleclick.net".
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
    // them.
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
