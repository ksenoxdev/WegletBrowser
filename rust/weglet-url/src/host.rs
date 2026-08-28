// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The host of an absolute URL. Not a parser: it finds the authority and
// trims it.
pub fn host(url: &str) -> Option<&str> {
    let (_, rest) = url.split_once("://")?;

    // A backslash ends the authority like a slash, so
    // "example.com\@evil.com" is host "example.com".
    let authority = rest.split(['/', '\\', '?', '#']).next()?;

    // rsplit, not split: userinfo comes before the host, so
    // "google.com@evil.example" is evil.example.
    let authority = authority.rsplit('@').next()?;

    let host = if authority.starts_with('[') {
        // IPv6 literal: the colons are part of the address, so the port
        // can only be after the closing bracket.
        let end = authority.find(']')?;
        &authority[..=end]
    } else {
        authority.split(':').next()?
    };

    (!host.is_empty()).then_some(host)
}

// Falls back to the whole string: an address bar that goes blank on a URL
// it cannot parse is worse than one showing something odd.
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
