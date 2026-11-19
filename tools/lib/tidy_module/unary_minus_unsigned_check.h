// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_UNARY_MINUS_UNSIGNED_CHECK_H_
#define TOOLS_TIDY_MODULE_UNARY_MINUS_UNSIGNED_CHECK_H_

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-unary-minus-unsigned: the built-in unary minus only on a signed
 *        type, CodingStyle T-25 and MISRA C++:2023 Rule 8.3.1.
 *
 * Reported: the built-in unary minus on an operand whose type, before the
 * promotion, is an unsigned integer, as `-count` of a `uint8_t` or `-1u`; an
 * unscoped enum of an unsigned underlying type as well. Not reported: `bool`
 * and plain `char`, which are no numbers; an overloaded `operator-`. In a
 * template it is reported in each instantiation whose operand is unsigned, as
 * the rule judges a template by its instantiations.
 *
 * No fix-it: whether the operand becomes signed or is cast, and to which
 * width, only the author knows.
 */
class UnaryMinusUnsignedCheck : public clang::tidy::ClangTidyCheck {
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

#endif  // TOOLS_TIDY_MODULE_UNARY_MINUS_UNSIGNED_CHECK_H_
