// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The chrome://weglet/ host, and URLs built from it.

#ifndef WEGLET_COMMON_WEGLET_HOST_H_
#define WEGLET_COMMON_WEGLET_HOST_H_

#include <string>
#include <string_view>

#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {

// Weglet's pages live at chrome://weglet/. The host and the page paths
// come from contract.json through the generated header, never a second
// copy here.
inline constexpr std::string_view kHost = contract::kHost;

// chrome://weglet/<path>
inline GURL PageUrl(std::string_view path) {
  return GURL(std::string(content::kChromeUIScheme) + "://" +
              std::string(kHost) + "/" + std::string(path));
}

}  // namespace weglet

#endif  // WEGLET_COMMON_WEGLET_HOST_H_
