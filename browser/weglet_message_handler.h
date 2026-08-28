// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Receives and validates the messages a page sends with chrome.send().

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

// The browser end of the channel to one of our pages. No allow-list
// here -- content grants chrome.send() only to a frame the
// WebUIControllerFactory claims -- but every message is still checked
// against contract.json's generated MessageSpec, since a compromised
// renderer is exactly what this boundary is for.
class WegletMessageHandler : public content::WebUIMessageHandler {
 public:
  // `bridge` and `state` outlive this handler. `window` may be null: a
  // page open outside a window has no tab model, and the messages that
  // need one are dropped.
  WegletMessageHandler(WegletWindow* window,
                       WegletBridge* bridge,
                       WegletStateService* state);
  WegletMessageHandler(const WegletMessageHandler&) = delete;
  WegletMessageHandler& operator=(const WegletMessageHandler&) = delete;
  ~WegletMessageHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Every message arrives here. `spec` is bound at registration.
  void OnMessage(const contract::MessageSpec* spec,
                 const base::ListValue& args);

  // True when `args` is exactly what `spec` says. Anything else is
  // dropped with a log line.
  static bool Validate(const contract::MessageSpec& spec,
                       const base::ListValue& args);

  // Called only after Validate has passed.
  void Dispatch(std::string_view name, const base::ListValue& args);

  // Accessors for validated arguments.
  static const std::string& StringAt(const base::ListValue& args, size_t index);
  static uint64_t TabIdAt(const base::ListValue& args, size_t index);
  static size_t IndexAt(const base::ListValue& args, size_t index);
  static bool BoolAt(const base::ListValue& args, size_t index);

  // The window this page is in, or null.
  const raw_ptr<WegletWindow> window_;
  const raw_ptr<WegletBridge> bridge_;
  const raw_ptr<WegletStateService> state_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_MESSAGE_HANDLER_H_
