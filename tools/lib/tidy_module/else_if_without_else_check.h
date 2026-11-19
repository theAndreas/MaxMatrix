// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_ELSE_IF_WITHOUT_ELSE_CHECK_H_
#define TOOLS_TIDY_MODULE_ELSE_IF_WITHOUT_ELSE_CHECK_H_

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-else-if-without-else: an `if … else if` chain ends with
 *        `else`, CodingStyle T-24 and MISRA C++:2023 Rule 9.4.1.
 *
 * Reported: the `if` of the last `else if` of a chain that has no `else`, an
 * `if constexpr` as well. Not reported: an `if` alone, with or without `else`;
 * an `if` nested in the branch of another, which starts a chain of its own.
 * Reported where it is written: in a template, not again in its
 * instantiations; not in the body of a macro, which every expansion shares.
 *
 * No fix-it: what the `else` does, or why it does nothing, only the author
 * knows.
 */
class ElseIfWithoutElseCheck : public clang::tidy::ClangTidyCheck {
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

#endif  // TOOLS_TIDY_MODULE_ELSE_IF_WITHOUT_ELSE_CHECK_H_
