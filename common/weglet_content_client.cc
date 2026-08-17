// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/common/weglet_content_client.cc

#include "weglet/common/weglet_content_client.h"

#include "base/memory/ref_counted_memory.h"
#include "ui/base/resource/resource_bundle.h"
#include "weglet/common/weglet_scheme.h"
#include "ui/gfx/image/image.h"

namespace weglet {

WegletContentClient::WegletContentClient() = default;
WegletContentClient::~WegletContentClient() = default;

std::u16string WegletContentClient::GetLocalizedString(int message_id) {
  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

std::string_view WegletContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

scoped_refptr<base::RefCountedMemory> WegletContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string WegletContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& WegletContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

void WegletContentClient::AddAdditionalSchemes(Schemes* schemes) {
  // standard_schemes: the URL parser then treats weglet://newtab/x.css as
  // having a host and a path, instead of one opaque blob. Without it the
  // loader factory has no host to switch on and a page cannot reference a
  // stylesheet beside it.
  schemes->standard_schemes.push_back(kWegletScheme);

  // secure_schemes: our pages come out of the binary, so they are at least
  // as trustworthy as https. This is what lets them use APIs gated on a
  // secure context, and stops the engine treating scripts on them as
  // mixed content.
  schemes->secure_schemes.push_back(kWegletScheme);

  // Deliberately NOT added to cors_enabled_schemes or
  // service_worker_schemes: nothing of ours fetches cross-origin or
  // registers a worker, and both would widen what a page of ours can
  // reach for no gain.
}

}  // namespace weglet
