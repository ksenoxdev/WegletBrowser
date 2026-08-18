// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_web_ui_controller_factory.h

#ifndef WEGLET_BROWSER_WEGLET_WEB_UI_CONTROLLER_FACTORY_H_
#define WEGLET_BROWSER_WEGLET_WEB_UI_CONTROLLER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace weglet {

// Decides which URLs are Weglet pages.
//
// This is where the privilege boundary is drawn. Content asks this factory
// about every navigation; the answer decides whether the frame gets the
// bindings that make chrome.send() exist. Say yes to the wrong URL and
// arbitrary content gets a channel into the browser, so the test is an
// exact host match on our own scheme and nothing else.
//
// Registered once at startup and never destroyed -- content keeps the
// pointer for the life of the process.
class WegletWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  // Registers the single instance with content. Idempotent.
  static void Register();

  WegletWebUIControllerFactory(const WegletWebUIControllerFactory&) = delete;
  WegletWebUIControllerFactory& operator=(const WegletWebUIControllerFactory&) =
      delete;

  // content::WebUIControllerFactory:
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;

 private:
  // Constructed and held by a NoDestructor in Register(), which needs access
  // to both. Private so nobody else can make a second one: content compares
  // factories by identity.
  friend class base::NoDestructor<WegletWebUIControllerFactory>;

  WegletWebUIControllerFactory() = default;
  ~WegletWebUIControllerFactory() override = default;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_WEB_UI_CONTROLLER_FACTORY_H_
