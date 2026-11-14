// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_NO_FRIEND_CHECK_H_
#define TOOLS_TIDY_MODULE_NO_FRIEND_CHECK_H_

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-no-friend: no `friend`, but an operator defined in the class, a
 *        hidden friend, CodingStyle C-12 and C-15.
 *
 * Reported: a friend class, a friend function or function template that is
 * not an operator, and a friend operator declared in the class but defined
 * outside it. In a class template it is reported once, as written.
 */
class NoFriendCheck : public clang::tidy::ClangTidyCheck {
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

#endif  // TOOLS_TIDY_MODULE_NO_FRIEND_CHECK_H_
