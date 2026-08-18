// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// rust/weglet-security/src/lib.rs
//
// Whether an address is worth warning about.
//
// Everything here is advisory. The heuristics are wrong in both
// directions, and nothing else in the browser depends on them being
// right -- a miss is a quality bug, not a hole. Their measured
// false-positive and false-negative rates live in tests/corpus.rs, so a
// change to a rule comes with a number rather than an opinion.

mod blocklist;
mod risk;

pub use blocklist::{
    canonical_host, is_blocked_host, matches_user_blocklist, set_blocklist_override,
    USER_BLOCK_REASON, USER_BLOCK_TITLE,
};
pub use risk::{
    assess_navigation, set_brand_rules_override, set_sensitive_words_override,
    NavigationRisk, RiskLevel,
};
