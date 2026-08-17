// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/common/weglet_scheme.h

#ifndef WEGLET_COMMON_WEGLET_SCHEME_H_
#define WEGLET_COMMON_WEGLET_SCHEME_H_

namespace weglet {

// The scheme Weglet's own pages are served under. Registered in every
// process type, because the renderer has to agree with the browser about
// what kind of URL this is -- if it does not, a weglet:// page ends up
// with an opaque origin and its own scripts count as cross-origin.
inline constexpr char kWegletScheme[] = "weglet";

}  // namespace weglet

#endif  // WEGLET_COMMON_WEGLET_SCHEME_H_
