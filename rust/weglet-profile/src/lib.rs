// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-profile/src/lib.rs
//
// Everything Weglet keeps on disk.
//
// Two rules hold everywhere in this crate. Every write goes through
// atomic::write, because a truncated settings file reads as "no settings"
// and silently throws away everything the user chose. And every read
// treats the file as untrusted input -- clamping, truncating and falling
// back rather than trusting a number that came off disk.

mod atomic;
mod error;
mod paths;
mod session;
mod settings;

pub use atomic::write as write_atomic;
pub use error::Error;
pub use paths::Paths;
pub use session::{Session, SessionTab};
pub use settings::{SearchEngine, Settings};
