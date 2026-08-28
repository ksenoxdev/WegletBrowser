// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Host handling for display and cheap comparisons. Anything that has to
// be correct against the URL Standard uses the `url` crate.

mod host;

pub use host::{display_host, host};
