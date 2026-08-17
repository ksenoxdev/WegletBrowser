// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-url/src/lib.rs
//
// Just enough URL handling for the things the browser shows the user.
// Anything that has to be correct against the URL Standard goes through
// the `url` crate instead; this is for display and for cheap host
// comparisons on hot paths.

mod host;

pub use host::{display_host, host};
