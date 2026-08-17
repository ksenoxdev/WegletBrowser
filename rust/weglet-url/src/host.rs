// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-url/src/host.rs

// The host part of an absolute URL, or None if there is no authority.
//
// Not a URL parser: it finds the authority and trims it. Everything that
// affects a security decision uses the `url` crate, which implements the
// actual standard.
pub fn host(url: &str) -> Option<&str> {
    let (_, rest) = url.split_once("://")?;

    // A backslash ends the authority exactly like a slash. The URL
    // Standard says so and browsers do it, so "example.com\@evil.com" is
    // host "example.com" here too -- reading it as "example.com\@evil.com"
    // would show a host the browser is not actually connected to.
    let authority = rest.split(['/', '\\', '?', '#']).next()?;

    // rsplit, not split: userinfo comes BEFORE the host, so the last
    // segment is the host. "user@example.com" is example.com, and
    // "google.com@evil.example" is evil.example -- which is the whole
    // point of stripping it.
    let authority = authority.rsplit('@').next()?;

    let host = if authority.starts_with('[') {
        // IPv6 literal: the colons inside are part of the address, so
        // the port can only be after the closing bracket.
        let end = authority.find(']')?;
        &authority[..=end]
    } else {
        authority.split(':').next()?
    };

    (!host.is_empty()).then_some(host)
}

// What to show the user when there is no better answer. Falls back to
// the whole string rather than an empty one: an address bar that goes
// blank on a URL it cannot parse is worse than one showing something odd.
pub fn display_host(url: &str) -> &str {
    host(url).unwrap_or(url)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_plain_url_yields_its_host() {
        assert_eq!(host("https://example.com/path"), Some("example.com"));
        assert_eq!(host("http://example.com"), Some("example.com"));
        assert_eq!(host("https://sub.example.co.uk/a/b?c=d#e"), Some("sub.example.co.uk"));
    }

    #[test]
    fn a_port_is_not_part_of_the_host() {
        assert_eq!(host("https://example.com:8443/"), Some("example.com"));
        assert_eq!(host("http://localhost:3000"), Some("localhost"));
    }

    #[test]
    fn an_ipv6_literal_keeps_its_brackets_and_loses_its_port() {
        assert_eq!(host("http://[::1]/"), Some("[::1]"));
        assert_eq!(host("http://[::1]:9229/x"), Some("[::1]"));
        assert_eq!(host("http://[2001:db8::1]/"), Some("[2001:db8::1]"));
    }

    // The host is what comes after the last @, not before the first one.
    #[test]
    fn userinfo_is_stripped_from_the_front() {
        assert_eq!(host("https://user@example.com/"), Some("example.com"));
        assert_eq!(host("https://user:pw@example.com/"), Some("example.com"));
        assert_eq!(
            host("https://google.com@evil.example/"),
            Some("evil.example")
        );
    }

    #[test]
    fn a_backslash_ends_the_authority_like_a_slash() {
        assert_eq!(host("https://example.com\\@evil.com"), Some("example.com"));
        assert_eq!(host("https://example.com\\path"), Some("example.com"));
        assert_eq!(host("https://example.com\\"), Some("example.com"));
    }

    #[test]
    fn an_address_without_an_authority_has_no_host() {
        assert_eq!(host("about:blank"), None);
        assert_eq!(host("example.com"), None);
        assert_eq!(host("https://"), None);
        assert_eq!(host(""), None);
    }

    #[test]
    fn display_host_falls_back_to_the_whole_string() {
        assert_eq!(display_host("https://example.com/x"), "example.com");
        assert_eq!(display_host("about:blank"), "about:blank");
        assert_eq!(display_host(""), "");
    }
}
