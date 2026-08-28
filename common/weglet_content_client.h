// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// ContentClient: resource-pack access for content and Blink.

#ifndef WEGLET_COMMON_WEGLET_CONTENT_CLIENT_H_
#define WEGLET_COMMON_WEGLET_CONTENT_CLIENT_H_

#include <string>
#include <string_view>

#include "content/public/common/content_client.h"

namespace weglet {

// Without a ContentClient, Blink cannot load the resources it parses HTML
// with, and every page renders as plain text.
class WegletContentClient : public content::ContentClient {
 public:
  WegletContentClient();
  WegletContentClient(const WegletContentClient&) = delete;
  WegletContentClient& operator=(const WegletContentClient&) = delete;
  ~WegletContentClient() override;

  // content::ContentClient:
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  scoped_refptr<base::RefCountedMemory> GetDataResourceBytes(
      int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
};

}  // namespace weglet

#endif  // WEGLET_COMMON_WEGLET_CONTENT_CLIENT_H_
