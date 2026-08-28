// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_permission_delegate.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "weglet/browser/weglet_window.h"

namespace weglet {

namespace {

// The only permission types this browser has UI to ask about. Anything
// else falls through to a flat deny.
bool IsPromptable(blink::PermissionType permission) {
  switch (permission) {
    case blink::PermissionType::GEOLOCATION:
    case blink::PermissionType::NOTIFICATIONS:
    case blink::PermissionType::AUDIO_CAPTURE:
    case blink::PermissionType::VIDEO_CAPTURE:
      return true;
    default:
      return false;
  }
}

// The id permission_prompt.ts keys its i18n strings and icon on.
std::string_view IdFor(blink::PermissionType permission) {
  switch (permission) {
    case blink::PermissionType::GEOLOCATION:
      return "location";
    case blink::PermissionType::NOTIFICATIONS:
      return "notifications";
    case blink::PermissionType::AUDIO_CAPTURE:
      return "microphone";
    case blink::PermissionType::VIDEO_CAPTURE:
      return "camera";
    default:
      return "";
  }
}

// The four IsPromptable() types, keyed the other way -- the reverse of
// IdFor(), for a decision made by id string (from the site-info popup)
// rather than by blink::PermissionType (from a page's own request).
std::optional<blink::PermissionType> TypeForId(std::string_view id) {
  if (id == "location") {
    return blink::PermissionType::GEOLOCATION;
  }
  if (id == "notifications") {
    return blink::PermissionType::NOTIFICATIONS;
  }
  if (id == "microphone") {
    return blink::PermissionType::AUDIO_CAPTURE;
  }
  if (id == "camera") {
    return blink::PermissionType::VIDEO_CAPTURE;
  }
  return std::nullopt;
}

}  // namespace

WegletPermissionDelegate::WegletPermissionDelegate() = default;
WegletPermissionDelegate::~WegletPermissionDelegate() = default;

blink::mojom::PermissionStatus WegletPermissionDelegate::StatusFor(
    const url::Origin& origin, blink::PermissionType permission) const {
  if (permission == blink::PermissionType::CLIPBOARD_SANITIZED_WRITE) {
    return blink::mojom::PermissionStatus::GRANTED;
  }
  if (!IsPromptable(permission)) {
    return blink::mojom::PermissionStatus::DENIED;
  }
  auto found = decisions_.find({origin, permission});
  if (found == decisions_.end()) {
    return blink::mojom::PermissionStatus::ASK;
  }
  return found->second ? blink::mojom::PermissionStatus::GRANTED
                       : blink::mojom::PermissionStatus::DENIED;
}

void WegletPermissionDelegate::Answer(
    const url::Origin& origin,
    const std::vector<blink::PermissionType>& types,
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
        callback) {
  std::vector<content::PermissionResult> result;
  for (blink::PermissionType type : types) {
    result.emplace_back(StatusFor(origin, type));
  }
  std::move(callback).Run(result);
}

void WegletPermissionDelegate::OnPromptAnswered(
    url::Origin origin,
    std::vector<blink::PermissionType> types,
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
        callback,
    bool allow) {
  for (blink::PermissionType type : types) {
    if (IsPromptable(type)) {
      decisions_[{origin, type}] = allow;
    }
  }
  Answer(origin, types, std::move(callback));
}

void WegletPermissionDelegate::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
        callback) {
  std::vector<blink::PermissionType> types;
  for (const auto& permission : request_description.permissions) {
    types.push_back(blink::PermissionDescriptorToPermissionType(permission));
  }
  const url::Origin origin = render_frame_host->GetLastCommittedOrigin();

  std::vector<std::string> prompt_ids;
  for (blink::PermissionType type : types) {
    if (IsPromptable(type) && !decisions_.contains({origin, type})) {
      prompt_ids.emplace_back(IdFor(type));
    }
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  WegletWindow* window = contents ? WegletWindow::FromWebContents(contents) : nullptr;
  if (prompt_ids.empty() || !window) {
    Answer(origin, types, std::move(callback));
    return;
  }

  window->ShowPermissionPrompt(
      origin.Serialize(), std::move(prompt_ids),
      base::BindOnce(&WegletPermissionDelegate::OnPromptAnswered,
                     weak_factory_.GetWeakPtr(), origin, types, std::move(callback)));
}

blink::mojom::PermissionStatus WegletPermissionDelegate::GetPermissionStatus(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return StatusFor(url::Origin::Create(requesting_origin),
                   blink::PermissionDescriptorToPermissionType(permission_descriptor));
}

content::PermissionResult
WegletPermissionDelegate::GetPermissionResultForOriginWithoutContext(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return content::PermissionResult(StatusFor(
      requesting_origin, blink::PermissionDescriptorToPermissionType(permission_descriptor)));
}

content::PermissionResult
WegletPermissionDelegate::GetPermissionResultForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return content::PermissionResult(
      StatusFor(render_frame_host->GetLastCommittedOrigin(),
               blink::PermissionDescriptorToPermissionType(permission_descriptor)));
}

content::PermissionResult
WegletPermissionDelegate::GetPermissionResultForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return content::PermissionResult(StatusFor(
      url::Origin::Create(worker_origin),
      blink::PermissionDescriptorToPermissionType(permission_descriptor)));
}

content::PermissionResult
WegletPermissionDelegate::GetPermissionResultForEmbeddedRequester(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  return content::PermissionResult(StatusFor(
      overridden_origin, blink::PermissionDescriptorToPermissionType(permission_descriptor)));
}

void WegletPermissionDelegate::ResetPermission(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  decisions_.erase({url::Origin::Create(requesting_origin), permission});
}

std::vector<WegletPermissionDelegate::Decision>
WegletPermissionDelegate::DecisionsFor(const url::Origin& origin) const {
  constexpr blink::PermissionType kPromptable[] = {
      blink::PermissionType::GEOLOCATION, blink::PermissionType::NOTIFICATIONS,
      blink::PermissionType::AUDIO_CAPTURE, blink::PermissionType::VIDEO_CAPTURE};
  std::vector<Decision> result;
  for (blink::PermissionType type : kPromptable) {
    auto found = decisions_.find({origin, type});
    std::string status = "ask";
    if (found != decisions_.end()) {
      status = found->second ? "granted" : "denied";
    }
    result.push_back({std::string(IdFor(type)), status});
  }
  return result;
}

void WegletPermissionDelegate::SetDecision(const url::Origin& origin,
                                           const std::string& id, bool allow) {
  std::optional<blink::PermissionType> type = TypeForId(id);
  if (type) {
    decisions_[{origin, *type}] = allow;
  }
}

bool WegletPermissionDelegate::IsMediaGranted(const url::Origin& origin,
                                              blink::PermissionType type) const {
  return StatusFor(origin, type) == blink::mojom::PermissionStatus::GRANTED;
}

void WegletPermissionDelegate::RequestMediaTypes(
    content::RenderFrameHost* render_frame_host,
    bool want_audio,
    bool want_video,
    base::OnceCallback<void(bool, bool)> callback) {
  const url::Origin origin = render_frame_host->GetLastCommittedOrigin();

  std::vector<std::string> prompt_ids;
  if (want_audio &&
      StatusFor(origin, blink::PermissionType::AUDIO_CAPTURE) ==
          blink::mojom::PermissionStatus::ASK) {
    prompt_ids.emplace_back(IdFor(blink::PermissionType::AUDIO_CAPTURE));
  }
  if (want_video &&
      StatusFor(origin, blink::PermissionType::VIDEO_CAPTURE) ==
          blink::mojom::PermissionStatus::ASK) {
    prompt_ids.emplace_back(IdFor(blink::PermissionType::VIDEO_CAPTURE));
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  WegletWindow* window = contents ? WegletWindow::FromWebContents(contents) : nullptr;

  if (prompt_ids.empty() || !window) {
    std::move(callback).Run(want_audio && IsMediaGranted(origin, blink::PermissionType::AUDIO_CAPTURE),
                            want_video && IsMediaGranted(origin, blink::PermissionType::VIDEO_CAPTURE));
    return;
  }

  window->ShowPermissionPrompt(
      origin.Serialize(), std::move(prompt_ids),
      base::BindOnce(&WegletPermissionDelegate::OnMediaPromptAnswered,
                     weak_factory_.GetWeakPtr(), origin, want_audio, want_video,
                     std::move(callback)));
}

void WegletPermissionDelegate::OnMediaPromptAnswered(
    url::Origin origin,
    bool want_audio,
    bool want_video,
    base::OnceCallback<void(bool, bool)> callback,
    bool allow) {
  if (want_audio) {
    decisions_[{origin, blink::PermissionType::AUDIO_CAPTURE}] = allow;
  }
  if (want_video) {
    decisions_[{origin, blink::PermissionType::VIDEO_CAPTURE}] = allow;
  }
  std::move(callback).Run(want_audio && allow, want_video && allow);
}

}  // namespace weglet
