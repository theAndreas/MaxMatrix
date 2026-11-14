// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "no_friend_check.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclFriend.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

namespace house {

void NoFriendCheck::registerMatchers(clang::ast_matchers::MatchFinder* finder) {
  using clang::ast_matchers::cxxRecordDecl;
  using clang::ast_matchers::friendDecl;
  using clang::ast_matchers::hasAncestor;
  using clang::ast_matchers::isTemplateInstantiation;
  using clang::ast_matchers::unless;
  // As written, not in each instantiation of a class template.
  finder->addMatcher(
      friendDecl(unless(hasAncestor(cxxRecordDecl(isTemplateInstantiation()))))
          .bind("friend"),
      this);
}

void NoFriendCheck::check(
    clang::ast_matchers::MatchFinder::MatchResult const& result) {
  auto const* const friend_decl =
      result.Nodes.getNodeAs<clang::FriendDecl>("friend");
  if (clang::NamedDecl const* const named = friend_decl->getFriendDecl()) {
    // A function, or the function of a function template.
    clang::FunctionDecl const* const function = named->getAsFunction();
    if (function != nullptr && function->isOverloadedOperator() &&
        function->isThisDeclarationADefinition()) {
      return;
    }
  }
  diag(friend_decl->getFriendLoc(),
       "friend declaration; reach the class through its public functions, a "
       "friend only as an operator defined in the class (CodingStyle C-12)");
}

}  // namespace house
