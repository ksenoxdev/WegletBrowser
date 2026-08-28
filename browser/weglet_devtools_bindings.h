// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Ported from content/shell/browser/shell_devtools_bindings.{h,cc} and
// shell_devtools_frontend.{h,cc}, merged into one class: content_shell
// splits them because a devtools front-end is its own top-level Shell
// window there, with a delegate/observer wrapper around the bindings.
// Weglet opens the front-end as an ordinary tab instead (see
// WegletWindow::OpenDevTools), so one class that is both the
// WebContentsObserver and the DevToolsAgentHostClient is enough.

#ifndef WEGLET_BROWSER_WEGLET_DEVTOOLS_BINDINGS_H_
#define WEGLET_BROWSER_WEGLET_DEVTOOLS_BINDINGS_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace weglet {

class WegletWindow;

class WegletDevToolsBindings : public content::WebContentsObserver,
                               public content::DevToolsAgentHostClient {
 public:
  // Self-deleting, same as content_shell's ShellDevToolsFrontend: lives
  // exactly as long as `devtools_contents`, and `owner` is told to close
  // that tab when the inspected page's agent host goes away.
  WegletDevToolsBindings(content::WebContents* devtools_contents,
                         content::WebContents* inspected_contents,
                         WegletWindow* owner);
  WegletDevToolsBindings(const WegletDevToolsBindings&) = delete;
  WegletDevToolsBindings& operator=(const WegletDevToolsBindings&) = delete;
  ~WegletDevToolsBindings() override;

  // Jumps to the element at (x, y) once attached -- x/y are in the
  // inspected frame's own viewport coordinates, same as ContextMenuParams.
  void InspectElementAt(int x, int y);

  bool MayAccessAllCookies() override;

 private:
  // content::DevToolsAgentHostClient:
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void WebContentsDestroyed() override;

  void AttachInternal();
  void HandleMessageFromDevToolsFrontend(base::DictValue message);
  void SendMessageAck(int request_id, base::DictValue arg);
  void CallClientFunction(
      const std::string& object_name,
      const std::string& method_name,
      base::Value arg1 = {},
      base::Value arg2 = {},
      base::Value arg3 = {},
      base::OnceCallback<void(base::Value)> cb = base::NullCallback());

  class NetworkResourceLoader;

  const raw_ptr<content::WebContents> inspected_contents_;
  const raw_ptr<WegletWindow> owner_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::unique_ptr<content::DevToolsFrontendHost> frontend_host_;
  int inspect_element_at_x_ = -1;
  int inspect_element_at_y_ = -1;

  std::set<std::unique_ptr<NetworkResourceLoader>, base::UniquePtrComparator>
      loaders_;
  base::DictValue preferences_;
  std::map<std::string, std::string> extensions_api_;

  base::WeakPtrFactory<WegletDevToolsBindings> weak_factory_{this};
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_DEVTOOLS_BINDINGS_H_
