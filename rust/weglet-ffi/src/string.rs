// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Strings across the boundary. Rust allocates, Rust frees.

use std::ffi::{c_char, CStr, CString};

// Hands a string to C++. Never returns null: a caller that has to check
// every string will forget once. An interior NUL is stripped rather than
// failing the call -- the only source is a page title.
pub fn into_c_string(value: String) -> *mut c_char {
    let cleaned: String = value.chars().filter(|c| *c != '\0').collect();
    match CString::new(cleaned) {
        Ok(string) => string.into_raw(),
        // Unreachable: NULs were filtered above.
        Err(_) => CString::default().into_raw(),
    }
}

// Borrows a C string. None for null or invalid UTF-8. Nothing in this
// crate keeps one past the call it arrived in.
//
/// # Safety
///
/// `value` must be null or point at a NUL-terminated string that stays
/// valid and unchanged for the duration of the call.
pub unsafe fn str_from_c<'a>(value: *const c_char) -> Option<&'a str> {
    if value.is_null() {
        return None;
    }
    // SAFETY: null-checked above; NUL-termination and validity are the
    // caller's contract.
    unsafe { CStr::from_ptr(value) }.to_str().ok()
}

// Frees a string that came from this library. C++ must not use free() or
// delete: it was allocated by Rust's allocator, which on Windows is a
// different heap.
//
/// # Safety
///
/// `value` must be null or a pointer returned by `into_c_string` and not
/// yet freed. Freeing it twice, or with C's `free`, is undefined.
#[no_mangle]
pub unsafe extern "C" fn weglet_string_free(value: *mut c_char) {
    if value.is_null() {
        return;
    }
    // SAFETY: null-checked; provenance is the caller's contract.
    drop(unsafe { CString::from_raw(value) });
}

#[cfg(test)]
mod tests {
    use super::*;

    fn round_trip(value: &str) -> String {
        let raw = into_c_string(value.to_string());
        // SAFETY: raw came from into_c_string on the line above.
        let borrowed = unsafe { str_from_c(raw) }.unwrap().to_string();
        // SAFETY: same pointer, freed exactly once.
        unsafe { weglet_string_free(raw) };
        borrowed
    }

    #[test]
    fn a_string_survives_the_round_trip() {
        assert_eq!(round_trip("https://example.com"), "https://example.com");
        assert_eq!(round_trip(""), "");
    }

    #[test]
    fn non_ascii_survives_the_round_trip() {
        assert_eq!(round_trip("\u{43F}\u{440}\u{438}\u{432}\u{435}\u{442}"),
                   "\u{43F}\u{440}\u{438}\u{432}\u{435}\u{442}");
        assert_eq!(round_trip("\u{1F600}"), "\u{1F600}");
    }

    // A page can put a NUL in its title.
    #[test]
    fn an_interior_nul_is_stripped_not_fatal() {
        assert_eq!(round_trip("a\0b"), "ab");
    }

    #[test]
    fn a_null_pointer_reads_as_none() {
        // SAFETY: null is explicitly allowed.
        assert!(unsafe { str_from_c(std::ptr::null()) }.is_none());
    }

    #[test]
    fn freeing_null_is_allowed() {
        // SAFETY: null is explicitly allowed.
        unsafe { weglet_string_free(std::ptr::null_mut()) };
    }

    #[test]
    fn into_c_string_never_returns_null() {
        let raw = into_c_string(String::new());
        assert!(!raw.is_null());
        // SAFETY: raw came from into_c_string.
        unsafe { weglet_string_free(raw) };
    }
}
