// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_spell_check_host.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace weglet {

WegletSpellCheckHost::WegletSpellCheckHost(WindowsSpellChecker* spell_checker)
    : spell_checker_(spell_checker) {}

WegletSpellCheckHost::~WegletSpellCheckHost() = default;

// static
void WegletSpellCheckHost::Create(
    WindowsSpellChecker* spell_checker,
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WegletSpellCheckHost>(spell_checker),
      std::move(receiver));
}

void WegletSpellCheckHost::NotifyChecked(const std::u16string& word,
                                         bool misspelled) {}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void WegletSpellCheckHost::CallSpellingService(
    const std::u16string& text, CallSpellingServiceCallback callback) {
  std::move(callback).Run(false, {});
}
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void WegletSpellCheckHost::RequestTextCheck(
    const std::u16string& text,
    const std::vector<spellcheck::SpellingMarker>& spelling_markers,
    RequestTextCheckCallback callback) {
  if (!spell_checker_) {
    std::move(callback).Run({});
    return;
  }
  spellcheck_platform::RequestTextCheck(
      spell_checker_, spellcheck_platform::GetDocumentTag(), text,
      std::move(callback));
}
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && BUILDFLAG(ENABLE_SPELLING_SERVICE)
void WegletSpellCheckHost::CheckSpelling(const std::u16string& word,
                                         CheckSpellingCallback callback) {
  // Matches spellcheck_platform_win.cc's own stub: the Windows native
  // checker has no synchronous single-word query, only RequestTextCheck.
  std::move(callback).Run(true);
}

void WegletSpellCheckHost::FillSuggestionList(
    const std::u16string& word, FillSuggestionListCallback callback) {
  std::move(callback).Run({});
}
#endif

#if BUILDFLAG(IS_WIN)
void WegletSpellCheckHost::InitializeDictionaries(
    InitializeDictionariesCallback callback) {
  // No Hunspell dictionary: a language entry with an invalid file marks
  // itself disabled on the renderer side (SpellcheckLanguage::IsEnabled),
  // which is exactly what routes checking to RequestTextCheck above
  // instead of a local Hunspell pass -- see
  // SpellCheckProvider::RequestTextCheckingFromBrowser's use_native check.
  // The language tag itself doesn't have to match a real installed
  // dictionary: RequestTextCheck above checks against every native
  // checker WegletBrowserMainParts created, not this one specifically.
  std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries;
  dictionaries.push_back(spellcheck::mojom::SpellCheckBDictLanguage::New(
      base::File(), "en-US"));
  std::move(callback).Run(std::move(dictionaries), /*custom_words=*/{},
                          /*enable=*/true);
}
#endif

}  // namespace weglet
