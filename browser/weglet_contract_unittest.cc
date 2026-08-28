// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The generated contract, checked from the side that consumes it:
// generators_test.py checks the text, this checks what it means once
// compiled. Most of it is constexpr, so most of these are static_asserts
// written as tests so a failure names itself.

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {
namespace {

using contract::ArgKind;
using contract::MessageSpec;
using contract::PageKind;

// --- pages ------------------------------------------------------------

TEST(WegletContractTest, EveryPageHasItsOwnKind) {
  // A duplicate kind would make KindForPath return the wrong one, and the
  // state service push the wrong payload.
  for (const contract::Page& page : contract::kPages) {
    EXPECT_EQ(page.kind, contract::KindForPath(page.path))
        << "page " << page.path << " does not map back to its own kind";
    EXPECT_NE(page.kind, PageKind::kOther);
    EXPECT_FALSE(page.path.empty());
  }
}

TEST(WegletContractTest, AnUnknownPathIsNotOneOfOurs) {
  // The paths decide which pages get profile state, so a near miss must
  // not be a hit.
  EXPECT_EQ(PageKind::kOther, contract::KindForPath(""));
  EXPECT_EQ(PageKind::kOther, contract::KindForPath("/newtab.html"));
  EXPECT_EQ(PageKind::kOther, contract::KindForPath("newtab.html.evil"));
  EXPECT_EQ(PageKind::kOther, contract::KindForPath("nonesuch.html"));
}

TEST(WegletContractTest, PathsAreUnique) {
  for (size_t i = 0; i < contract::kPages.size(); ++i) {
    for (size_t j = i + 1; j < contract::kPages.size(); ++j) {
      EXPECT_NE(contract::kPages[i].path, contract::kPages[j].path);
      EXPECT_NE(contract::kPages[i].kind, contract::kPages[j].kind);
    }
  }
}

// --- pushes -----------------------------------------------------------

TEST(WegletContractTest, EveryPushTargetsARealPage) {
  for (const contract::Push& push : contract::kPushes) {
    EXPECT_NE(push.page, PageKind::kOther);
    EXPECT_FALSE(push.function.empty());
    EXPECT_EQ(push.function, contract::PushFunctionFor(push.page));
  }
}

TEST(WegletContractTest, APageWithNothingToPushGetsAnEmptyFunction) {
  // The state service uses an empty answer to mean "nothing to send".
  EXPECT_TRUE(contract::PushFunctionFor(PageKind::kOther).empty());
}

TEST(WegletContractTest, PushFunctionsAreUnique) {
  // Two pushes with one name would install over each other on the page.
  for (size_t i = 0; i < contract::kPushes.size(); ++i) {
    for (size_t j = i + 1; j < contract::kPushes.size(); ++j) {
      EXPECT_NE(contract::kPushes[i].function, contract::kPushes[j].function);
      EXPECT_NE(contract::kPushes[i].page, contract::kPushes[j].page);
    }
  }
}

// --- messages ---------------------------------------------------------

TEST(WegletContractTest, MessageNamesAreUniqueAndNonEmpty) {
  // A duplicate would register twice, and content ignores the second.
  for (size_t i = 0; i < contract::kMessages.size(); ++i) {
    EXPECT_FALSE(contract::kMessages[i].name.empty());
    for (size_t j = i + 1; j < contract::kMessages.size(); ++j) {
      EXPECT_NE(contract::kMessages[i].name, contract::kMessages[j].name);
    }
  }
}

TEST(WegletContractTest, EveryArityFitsTheArgumentArray) {
  // The handler reads args[0..arity), so an arity past the array would
  // read past the end.
  for (const MessageSpec& spec : contract::kMessages) {
    EXPECT_LE(spec.arity, contract::kMaxMessageArgs) << spec.name;
    EXPECT_LE(spec.arity, spec.args.size()) << spec.name;
  }
}

// The messages the browser answers but does not act on. Registered on
// purpose: an unregistered chrome.send() crashes the browser process.
TEST(WegletContractTest, UnimplementedMessagesAreStillDeclared) {
  size_t unimplemented = 0;
  for (const MessageSpec& spec : contract::kMessages) {
    if (!spec.implemented) {
      ++unimplemented;
      EXPECT_FALSE(spec.name.empty());
    }
  }
  // Not a fixed number: this shrinks as features land. What matters is
  // that some are marked, so the handler's branch is not dead.
  EXPECT_GT(contract::kMessages.size(), unimplemented)
      << "every message is unimplemented, which cannot be right";
}

// --- internal addresses -----------------------------------------------

TEST(WegletContractTest, EveryInternalAddressResolvesToARealPage) {
  // The failure this list was created for: three addresses existed on the
  // Rust side with nothing here to resolve them.
  for (const contract::InternalAddress& entry : contract::kInternalAddresses) {
    EXPECT_FALSE(entry.address.empty());
    EXPECT_NE(PageKind::kOther, contract::KindForPath(entry.page))
        << entry.address << " resolves to a page that is not one of ours";
  }
}

TEST(WegletContractTest, InternalAddressesAreUnique) {
  for (size_t i = 0; i < contract::kInternalAddresses.size(); ++i) {
    for (size_t j = i + 1; j < contract::kInternalAddresses.size(); ++j) {
      EXPECT_NE(contract::kInternalAddresses[i].address,
                contract::kInternalAddresses[j].address);
    }
  }
}

// --- the argument check the handler performs --------------------------
//
// A copy of WegletMessageHandler::Validate's rule -- the handler's own is
// private and needs a WebUI -- exercised against the real specs. An
// ArgKind nothing can produce would be a message no page could send.

bool Accepts(ArgKind kind, const base::Value& value) {
  switch (kind) {
    case ArgKind::kString:
      return value.GetIfString() != nullptr;
    case ArgKind::kNumber:
      return value.GetIfInt().value_or(-1) >= 0;
    case ArgKind::kBoolean:
      return value.GetIfBool().has_value();
    case ArgKind::kTabId: {
      const std::string* text = value.GetIfString();
      uint64_t ignored = 0;
      return text && base::StringToUint64(*text, &ignored);
    }
  }
  return false;
}

base::Value Sample(ArgKind kind) {
  switch (kind) {
    case ArgKind::kString:
      return base::Value("text");
    case ArgKind::kNumber:
      return base::Value(0);
    case ArgKind::kBoolean:
      return base::Value(true);
    case ArgKind::kTabId:
      return base::Value("18446744073709551615");
  }
  return base::Value();
}

TEST(WegletContractTest, EverySpecCanBeSatisfied) {
  for (const MessageSpec& spec : contract::kMessages) {
    for (size_t index = 0; index < spec.arity; ++index) {
      const base::Value sample = Sample(spec.args[index]);
      EXPECT_TRUE(Accepts(spec.args[index], sample))
          << spec.name << " argument " << index;
    }
  }
}

TEST(WegletContractTest, ATabIdMustParseAsOne) {
  // Why ids cross as strings: a JavaScript number is a double and cannot
  // hold every u64 exactly.
  EXPECT_TRUE(Accepts(ArgKind::kTabId, base::Value("0")));
  EXPECT_TRUE(Accepts(ArgKind::kTabId, base::Value("18446744073709551615")));
  EXPECT_FALSE(Accepts(ArgKind::kTabId, base::Value("")));
  EXPECT_FALSE(Accepts(ArgKind::kTabId, base::Value("-1")));
  EXPECT_FALSE(Accepts(ArgKind::kTabId, base::Value("12abc")));
  EXPECT_FALSE(Accepts(ArgKind::kTabId, base::Value(12)));
  // One past u64.
  EXPECT_FALSE(Accepts(ArgKind::kTabId, base::Value("18446744073709551616")));
}

TEST(WegletContractTest, ANumberIsAPositionAndCannotBeNegative) {
  // Every number in the contract is an index into a list the page was
  // shown; a negative one would become a very large size_t.
  EXPECT_TRUE(Accepts(ArgKind::kNumber, base::Value(0)));
  EXPECT_TRUE(Accepts(ArgKind::kNumber, base::Value(7)));
  EXPECT_FALSE(Accepts(ArgKind::kNumber, base::Value(-1)));
  EXPECT_FALSE(Accepts(ArgKind::kNumber, base::Value("3")));
  EXPECT_FALSE(Accepts(ArgKind::kNumber, base::Value(true)));
}

TEST(WegletContractTest, TypesAreNotInterchangeable) {
  EXPECT_FALSE(Accepts(ArgKind::kString, base::Value(1)));
  EXPECT_FALSE(Accepts(ArgKind::kBoolean, base::Value(1)));
  EXPECT_FALSE(Accepts(ArgKind::kBoolean, base::Value("true")));
  EXPECT_FALSE(Accepts(ArgKind::kString, base::Value()));
}

}  // namespace
}  // namespace weglet
