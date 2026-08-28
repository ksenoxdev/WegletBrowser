// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The browser side of spellcheck::mojom::SpellCheckHost, bound per frame
// (see WegletContentBrowserClient::RegisterBrowserInterfaceBindersForFrame).
// Routes text checking through the Windows native spellchecker instead of
// bundling or downloading Hunspell dictionaries -- see InitializeDictionaries
// below for how the renderer is told to route checking here.
//
// Implements the mojom interface directly rather than subclassing
// components/spellcheck/browser's SpellCheckHostImpl: that class's own
// GN target pulls in //components/enterprise/connectors/core, which links
// an in-tree Rust crate -- and two independently built copies of Rust's
// std (that one and weglet_ffi's own cargo-built copy) collide at link
// time with a duplicate rust_eh_personality symbol.

#ifndef WEGLET_BROWSER_WEGLET_SPELL_CHECK_HOST_H_
#define WEGLET_BROWSER_WEGLET_SPELL_CHECK_HOST_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class WindowsSpellChecker;

namespace weglet {

class WegletSpellCheckHost : public spellcheck::mojom::SpellCheckHost {
 public:
  explicit WegletSpellCheckHost(WindowsSpellChecker* spell_checker);
  WegletSpellCheckHost(const WegletSpellCheckHost&) = delete;
  WegletSpellCheckHost& operator=(const WegletSpellCheckHost&) = delete;
  ~WegletSpellCheckHost() override;

  static void Create(
      WindowsSpellChecker* spell_checker,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver);

 private:
  // spellcheck::mojom::SpellCheckHost:
  void NotifyChecked(const std::u16string& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const std::u16string& text,
                           CallSpellingServiceCallback callback) override;
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void RequestTextCheck(
      const std::u16string& text,
      const std::vector<spellcheck::SpellingMarker>& spelling_markers,
      RequestTextCheckCallback callback) override;
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && BUILDFLAG(ENABLE_SPELLING_SERVICE)
  void CheckSpelling(const std::u16string& word,
                     CheckSpellingCallback callback) override;
  void FillSuggestionList(const std::u16string& word,
                          FillSuggestionListCallback callback) override;
#endif

#if BUILDFLAG(IS_WIN)
  void InitializeDictionaries(InitializeDictionariesCallback callback) override;
#endif

  // Not owned; outlives every WebContents, and every WegletSpellCheckHost
  // with it -- see WegletBrowserMainParts::spell_checker().
  raw_ptr<WindowsSpellChecker> spell_checker_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_SPELL_CHECK_HOST_H_
