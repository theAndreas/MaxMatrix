// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The module house: checks of the rules every project made from the template
// keeps, which clang-tidy's own checks do not. tools/lib/tidy_module.sh builds it,
// and tools/tidy.sh loads it with --load; .clang-tidy turns its checks on as
// house-*.

#include "bitwise_in_bit_class_check.h"
#include "braced_member_init_check.h"
#include "braced_var_init_check.h"
#include "clang-tidy/ClangTidyModule.h"
#include "no_friend_check.h"
#include "operator_form_check.h"

namespace house {
namespace {

class HouseModule : public clang::tidy::ClangTidyModule {
 public:
  void addCheckFactories(
      clang::tidy::ClangTidyCheckFactories& factories) override {
    factories.registerCheck<BitwiseInBitClassCheck>(
        "house-bitwise-in-bit-class");
    factories.registerCheck<BracedMemberInitCheck>("house-braced-member-init");
    factories.registerCheck<BracedVarInitCheck>("house-braced-var-init");
    factories.registerCheck<NoFriendCheck>("house-no-friend");
    factories.registerCheck<OperatorFormCheck>("house-operator-form");
  }
};

}  // namespace
}  // namespace house

// Registered when clang-tidy loads the module, by the constructor of this
// object.
static clang::tidy::ClangTidyModuleRegistry::Add<house::HouseModule> const
    kModule{
        "house",
        "The checks of the rules every project made from the template keeps."};
