// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_OPERATOR_FORM_CHECK_H_
#define TOOLS_TIDY_MODULE_OPERATOR_FORM_CHECK_H_

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-operator-form: an operator is a member or a hidden friend, by
 *        what it does, CodingStyle C-15.
 *
 * - A binary operator that leaves its operands as they are, `+`, `==`, `<`,
 *   `<<` and the others, is a hidden friend: reported as a member, and as a
 *   free function that takes a class of the same file, which can be its
 *   friend. One for a class of another file, as a test's `operator<<` that
 *   prints a type of the library, is not reported, nor one for an `enum`,
 *   which can have no friend.
 * - An operator that changes its operand, `+=` and its like, `++` and `--`,
 *   and a unary one, `-x`, `!x`, `~x`, `*x`, `&x`, is a member: reported as a
 *   friend, and as a free function whose first parameter is a class of the
 *   same file.
 *
 * What the language makes a member, `=`, `[]`, `()`, `->` and a conversion,
 * is one anyway. Whether a friend is defined in the class is house-no-friend's
 * to report. Reported at the first declaration, once for a template.
 */
class OperatorFormCheck : public clang::tidy::ClangTidyCheck {
 public:
  using ClangTidyCheck::ClangTidyCheck;

  bool isLanguageVersionSupported(
      clang::LangOptions const& lang_opts) const override {
    return lang_opts.CPlusPlus;
  }
  void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
  void check(
      clang::ast_matchers::MatchFinder::MatchResult const& result) override;
};

}  // namespace house

#endif  // TOOLS_TIDY_MODULE_OPERATOR_FORM_CHECK_H_
