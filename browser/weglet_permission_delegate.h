// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Always grants sanitized clipboard write, which is what the toolbar's
// own copy-address button needs. Without a delegate at all, content
// denies every permission unconditionally (see WegletBrowserContext),
// which also silently broke that button.
//
// Camera, microphone, location and notifications go through
// WegletPermissionPrompt instead and are remembered here for the rest of
// the browser session (not persisted across restarts). Everything else
// is denied outright -- the long tail of permission types (MIDI, NFC,
// idle detection, and so on) has no UI here to ask through.

#ifndef WEGLET_BROWSER_WEGLET_PERMISSION_DELEGATE_H_
#define WEGLET_BROWSER_WEGLET_PERMISSION_DELEGATE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

namespace weglet {

class WegletPermissionDelegate : public content::PermissionControllerDelegate {
 public:
  WegletPermissionDelegate();
  WegletPermissionDelegate(const WegletPermissionDelegate&) = delete;
  WegletPermissionDelegate& operator=(const WegletPermissionDelegate&) = delete;
  ~WegletPermissionDelegate() override;

  // content::PermissionControllerDelegate:
  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
          callback) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  content::PermissionResult GetPermissionResultForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      content::RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;
  content::PermissionResult GetPermissionResultForWorker(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  content::PermissionResult GetPermissionResultForEmbeddedRequester(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& overridden_origin) override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

  // For the toolbar's site-info popup.
  struct Decision {
    std::string id;
    // "granted", "denied", or "ask" -- never decided.
    std::string status;
  };
  std::vector<Decision> DecisionsFor(const url::Origin& origin) const;
  // `id` is one of the strings IdFor produces ("camera" and so on); a
  // request for a type this browser has no UI for is a silent no-op.
  void SetDecision(const url::Origin& origin, const std::string& id, bool allow);

  // getUserMedia() does not go through RequestPermissionsFromCurrentDocument
  // above at all -- content routes actual camera/mic device access through
  // WebContentsDelegate::RequestMediaAccessPermission/
  // CheckMediaAccessPermission instead (see their own doc comments), a
  // separate path this browser wires straight back into the same prompt
  // and cache.
  void RequestMediaTypes(
      content::RenderFrameHost* render_frame_host,
      bool want_audio,
      bool want_video,
      base::OnceCallback<void(bool audio_granted, bool video_granted)> callback);
  bool IsMediaGranted(const url::Origin& origin, blink::PermissionType type) const;

 private:
  blink::mojom::PermissionStatus StatusFor(const url::Origin& origin,
                                           blink::PermissionType permission) const;

  // Answers `callback` for every type in `types` against whatever is
  // decided (cached or not) right now -- the common tail of both the
  // no-prompt-needed path and a prompt's own answer.
  void Answer(
      const url::Origin& origin,
      const std::vector<blink::PermissionType>& types,
      base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
          callback);

  void OnPromptAnswered(
      url::Origin origin,
      std::vector<blink::PermissionType> types,
      base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
          callback,
      bool allow);

  void OnMediaPromptAnswered(
      url::Origin origin,
      bool want_audio,
      bool want_video,
      base::OnceCallback<void(bool, bool)> callback,
      bool allow);

  std::map<std::pair<url::Origin, blink::PermissionType>, bool> decisions_;

  base::WeakPtrFactory<WegletPermissionDelegate> weak_factory_{this};
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_PERMISSION_DELEGATE_H_
