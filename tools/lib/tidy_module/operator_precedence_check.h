// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_OPERATOR_PRECEDENCE_CHECK_H_
#define TOOLS_TIDY_MODULE_OPERATOR_PRECEDENCE_CHECK_H_

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-operator-precedence: an operand that is an operation of higher
 *        precedence stands in parentheses, `(a == b) && ready`, CodingStyle
 *        T-23 and MISRA C++:2023 Rule 8.0.1.
 *
 * By the ranks of the rule: reported is an operand of a binary operator, built
 * in or overloaded, or the condition or a branch of `?:`, that is itself a
 * binary operator or a `?:` of higher precedence; and the operand of `sizeof`
 * without parentheses. Not reported: an operand of the same precedence,
 * `a + b + c`; one that is a unary operator, a cast, a call or a name; and the
 * operands of an assignment, the comma, `throw` and a pointer to member.
 *
 * The fix-it puts the operand in parentheses. Reported where it is written: in
 * a template, not again in its instantiations; in the argument of a macro, but
 * not in the body of one, which the fix-it could not change at that place. The
 * expression of an `#if`, which the rule covers as well, is the
 * preprocessor's and out of reach of clang-tidy.
 */
class OperatorPrecedenceCheck : public clang::tidy::ClangTidyCheck {
 public:
  using ClangTidyCheck::ClangTidyCheck;

  bool isLanguageVersionSupported(
      clang::LangOptions const& lang_opts) const override {
    return lang_opts.CPlusPlus;
  }
  void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
  void check(
      clang::ast_matchers::MatchFinder::MatchResult const& result) override;

 private:
  // Reports `operand` of `parent`, with the parentheses as its fix-it.
  void Parenthesize(clang::Expr const& operand, clang::Expr const& parent,
                    clang::SourceManager const& manager);
  // Reports the operand of `sizeof` that stands without parentheses.
  void CheckSizeof(clang::UnaryExprOrTypeTraitExpr const& size,
                   clang::SourceManager const& manager);
};

}  // namespace house

#endif  // TOOLS_TIDY_MODULE_OPERATOR_PRECEDENCE_CHECK_H_
