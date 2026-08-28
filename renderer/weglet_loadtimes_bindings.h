// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// window.chrome.loadTimes()/csi(): real Chrome installs these on every page
// with real navigation-timing data. Some bot-detection scripts probe for
// them -- a Chrome UA with no window.chrome is a tell -- so weglet needs
// them present with real data, not just the name.
//
// A near-verbatim port of chrome/renderer/loadtimes_bindings.h: nothing
// here is chrome:: or extensions-specific.

#ifndef WEGLET_RENDERER_WEGLET_LOADTIMES_BINDINGS_H_
#define WEGLET_RENDERER_WEGLET_LOADTIMES_BINDINGS_H_

#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

namespace weglet {

class WegletLoadTimesBindings : public gin::Wrappable<WegletLoadTimesBindings> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                     gin::kLoadTimesBindings};

  WegletLoadTimesBindings(const WegletLoadTimesBindings&) = delete;
  WegletLoadTimesBindings& operator=(const WegletLoadTimesBindings&) = delete;

  static void Install(v8::Local<v8::Context> context);

  WegletLoadTimesBindings();
  ~WegletLoadTimesBindings() override;

 private:
  static void LoadTimesCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void CSICallback(const v8::FunctionCallbackInfo<v8::Value>& info);

  // gin::WrappableBase
  const gin::WrapperInfo* wrapper_info() const override;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  v8::Local<v8::Value> GetLoadTimes(v8::Isolate* isolate);
  v8::Local<v8::Value> GetCSI(v8::Isolate* isolate);
};

}  // namespace weglet

#endif  // WEGLET_RENDERER_WEGLET_LOADTIMES_BINDINGS_H_
