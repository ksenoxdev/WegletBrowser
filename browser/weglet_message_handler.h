// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_message_handler.h

#ifndef WEGLET_BROWSER_WEGLET_MESSAGE_HANDLER_H_
#define WEGLET_BROWSER_WEGLET_MESSAGE_HANDLER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {

class WegletBridge;
class WegletStateService;
class WegletWindow;

// The browser end of the channel to one of our pages.
//
// This is the trust boundary, and it is the engine that enforces it, not
// this class: content grants the bindings that make chrome.send() exist
// only to a frame whose URL our WebUIControllerFactory claims. Ordinary
// web content has no way to reach any of the methods below -- there is no
// allow-list here to get wrong, because a page that is not ours never gets
// a handler in the first place.
//
// Arguments are still checked, every message, every time. The page is our
// own code, but it runs in a renderer process, and a compromised renderer
// is exactly the case this boundary exists for. What changed is that the
// check is no longer hand-written per handler: the arity and the type of
// every argument come from weglet/ui/contract.json through the generated
// MessageSpec, so changing a message's signature is a compile-time fact on
// both sides instead of a TypeScript error and silence here.
class WegletMessageHandler : public content::WebUIMessageHandler {
 public:
  // `bridge` and `state` outlive this handler -- both belong to the
  // browser process's own objects. `window` may be null: a page open in a
  // tab of its own is not attached to a window's tab model, and the
  // messages that need one are dropped rather than dereferencing null.
  WegletMessageHandler(WegletWindow* window,
                       WegletBridge* bridge,
                       WegletStateService* state);
  WegletMessageHandler(const WegletMessageHandler&) = delete;
  WegletMessageHandler& operator=(const WegletMessageHandler&) = delete;
  ~WegletMessageHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Every message arrives here. `spec` is the generated description of the
  // message being handled, bound at registration.
  void OnMessage(const contract::MessageSpec* spec,
                 const base::ListValue& args);

  // True when `args` is exactly what `spec` says the message takes. A
  // message with the wrong shape is dropped with a log line rather than
  // trusted.
  static bool Validate(const contract::MessageSpec& spec,
                       const base::ListValue& args);

  // Called only after Validate has passed, so each of these may read the
  // arguments the spec promised without checking again.
  void Dispatch(std::string_view name, const base::ListValue& args);

  // Accessors for validated arguments.
  static const std::string& StringAt(const base::ListValue& args, size_t index);
  static uint64_t TabIdAt(const base::ListValue& args, size_t index);
  static size_t IndexAt(const base::ListValue& args, size_t index);
  static bool BoolAt(const base::ListValue& args, size_t index);

  // The window this page is in, or null. Only the tab and navigation
  // messages need it.
  const raw_ptr<WegletWindow> window_;
  const raw_ptr<WegletBridge> bridge_;
  const raw_ptr<WegletStateService> state_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_MESSAGE_HANDLER_H_
