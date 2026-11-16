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

// The class a type names: a class, the injected name of a class template, or a
// specialization of a class template, by that template's pattern where it
// depends on a parameter, as `Length<Unit>` in the template `Length`.
clang::CXXRecordDecl const* ClassOf(clang::QualType type) {
  clang::CXXRecordDecl const* record{type->getAsCXXRecordDecl()};
  if (record == nullptr) {
    if (auto const* specialization =
            type->getAs<clang::TemplateSpecializationType>()) {
      auto const* const pattern{
          llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
              specialization->getTemplateName().getAsTemplateDecl())};
      record = pattern == nullptr ? nullptr : pattern->getTemplatedDecl();
    }
  }
  return record == nullptr ? nullptr : record->getCanonicalDecl();
}

// The class template a type is a specialization of, as written, by its
// canonical declaration; null for any other type.
clang::Decl const* TemplateOf(clang::QualType type) {
  auto const* const specialization{
      type->getAs<clang::TemplateSpecializationType>()};
  clang::TemplateDecl const* const pattern{
      specialization == nullptr
          ? nullptr
          : specialization->getTemplateName().getAsTemplateDecl()};
  return pattern == nullptr ? nullptr : pattern->getCanonicalDecl();
}

// Whether `friend_decl` names the class's own CRTP base: a specialization of
// the class template of one of its direct bases, with the class itself among
// its arguments. The derived side of the pattern IsOwnTypeParameter() lets
// pass: the base calls the hooks the class keeps private to everyone else.
bool IsOwnCrtpBase(clang::FriendDecl const& friend_decl) {
  clang::TypeSourceInfo const* const written{friend_decl.getFriendType()};
  auto const* const self{
      llvm::dyn_cast<clang::CXXRecordDecl>(friend_decl.getDeclContext())};
  if (written == nullptr || self == nullptr || !self->hasDefinition()) {
    return false;
  }
  clang::QualType const friend_type{written->getType()};
  clang::Decl const* const base_template{TemplateOf(friend_type)};
  if (base_template == nullptr) {
    return false;
  }
  bool is_a_base{false};
  for (clang::CXXBaseSpecifier const& base : self->bases()) {
    is_a_base = is_a_base || TemplateOf(base.getType()) == base_template;
  }
  bool names_self{false};
  for (clang::TemplateArgument const& argument :
       friend_type->getAs<clang::TemplateSpecializationType>()
           ->template_arguments()) {
    bool const is_type{argument.getKind() == clang::TemplateArgument::Type};
    names_self = names_self || (is_type && ClassOf(argument.getAsType()) ==
                                               self->getCanonicalDecl());
  }
  return is_a_base && names_self;
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
  if (IsOwnTypeParameter(*friend_decl) || IsOwnCrtpBase(*friend_decl)) {
    return;
  }
  diag(friend_decl->getFriendLoc(),
       "friend declaration; reach the class through its public functions, a "
       "friend only as an operator defined in the class (CodingStyle C-12)");
}

}  // namespace house
