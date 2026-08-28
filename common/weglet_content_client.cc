// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// ContentClient: resource-pack access for content and Blink.

#include "weglet/common/weglet_content_client.h"

#include "base/memory/ref_counted_memory.h"
#include "ui/base/resource/resource_bundle.h"
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

}  // namespace weglet
