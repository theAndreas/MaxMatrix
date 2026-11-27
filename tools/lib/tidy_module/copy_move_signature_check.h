// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_COPY_MOVE_SIGNATURE_CHECK_H_
#define TOOLS_TIDY_MODULE_COPY_MOVE_SIGNATURE_CHECK_H_

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-copy-move-signature: a copy or move function written by hand
 *        has the signature MISRA C++:2023 Rule 15.0.2 gives, CodingStyle C-17.
 *
 * For a class `X`: `X(X const&)`, `X(X&&) noexcept`,
 * `X& operator=(X const&) &` and `X& operator=(X&&) & noexcept`.
 *
 * Reported, for a user-provided one, which is one not defaulted or deleted
 * where it is first declared: an assignment without the ref-qualifier `&`, or
 * with `&&`; a move without `noexcept`, or with `noexcept(false)`; a copy whose
 * parameter is no `X const&`, as `X&` or `X` by value, and a move whose
 * parameter is `X const&&`. A noexcept that depends on a template passes. The
 * return type is misc-unconventional-assign-operator's.
 *
 * Unlike performance-noexcept-move-constructor, it reports a missing noexcept
 * under `-fno-exceptions` as well, as a small target builds.
 *
 * In a class template it is reported once, at the template. A fix-it adds the
 * `&` and the `noexcept` where the function is declared once, as it is
 * defined; none where a second declaration would have to change as well, and
 * none for a parameter, whose form only the author knows.
 */
class CopyMoveSignatureCheck : public clang::tidy::ClangTidyCheck {
 public:
  using ClangTidyCheck::ClangTidyCheck;

  bool isLanguageVersionSupported(
      clang::LangOptions const& lang_opts) const override {
    return lang_opts.CPlusPlus11;
  }
  void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
  void check(
      clang::ast_matchers::MatchFinder::MatchResult const& result) override;
};

}  // namespace house

#endif  // TOOLS_TIDY_MODULE_COPY_MOVE_SIGNATURE_CHECK_H_
