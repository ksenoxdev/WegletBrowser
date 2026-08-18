// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/common/weglet_host.h

#ifndef WEGLET_COMMON_WEGLET_HOST_H_
#define WEGLET_COMMON_WEGLET_HOST_H_

#include <string>
#include <string_view>

#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {

// Weglet's own pages live at chrome://weglet/, which is content's WebUI
// scheme with our host on it -- WebUI is what gives a page a message
// channel to the browser, and content grants those bindings only on its
// own scheme.
//
// The host and every page path come from weglet/ui/contract.json through
// the generated header, not from a second copy here -- the pages listed
// there and the pages this browser can actually navigate to have to be
// the same five, and they went out of step once: three of the addresses
// the tab model knows about had nowhere to resolve to.
inline constexpr std::string_view kHost = contract::kHost;

// chrome://weglet/<path>
inline GURL PageUrl(std::string_view path) {
  return GURL(std::string(content::kChromeUIScheme) + "://" +
              std::string(kHost) + "/" + std::string(path));
}

}  // namespace weglet

#endif  // WEGLET_COMMON_WEGLET_HOST_H_
