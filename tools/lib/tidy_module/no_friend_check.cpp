// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "no_friend_check.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclFriend.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

namespace house {

namespace {

// Whether `friend_decl` names a type parameter of the class template it lies
// in: a CRTP base befriends its derived class so, the one class that may make
// it, as bugprone-crtp-constructor-accessibility asks. A parameter of an
// enclosing template is another class's and does not count.
bool IsOwnTypeParameter(clang::FriendDecl const& friend_decl) {
  clang::TypeSourceInfo const* const written{friend_decl.getFriendType()};
  if (written == nullptr) {
    return false;
  }
  auto const* const parameter{
      written->getType()->getAs<clang::TemplateTypeParmType>()};
  return parameter != nullptr && parameter->getDecl() != nullptr &&
         parameter->getDecl()->getDeclContext() ==
             friend_decl.getDeclContext();
}

}  // namespace

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
  if (IsOwnTypeParameter(*friend_decl)) {
    return;
  }
  diag(friend_decl->getFriendLoc(),
       "friend declaration; reach the class through its public functions, a "
       "friend only as an operator defined in the class (CodingStyle C-12)");
}

}  // namespace house
