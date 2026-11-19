// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "else_if_without_else_check.h"

#include "clang/AST/Stmt.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

namespace house {

void ElseIfWithoutElseCheck::registerMatchers(
    clang::ast_matchers::MatchFinder* finder) {
  using clang::ast_matchers::hasElse;
  using clang::ast_matchers::hasParent;
  using clang::ast_matchers::ifStmt;
  using clang::ast_matchers::isInTemplateInstantiation;
  using clang::ast_matchers::stmt;
  using clang::ast_matchers::unless;
  // An if without else in an if; check() keeps the one that is the else of
  // its parent, not its then.
  finder->addMatcher(
      ifStmt(unless(hasElse(stmt())), hasParent(ifStmt().bind("parent")),
             unless(isInTemplateInstantiation()))
          .bind("last"),
      this);
}

void ElseIfWithoutElseCheck::check(
    clang::ast_matchers::MatchFinder::MatchResult const& result) {
  auto const* const last = result.Nodes.getNodeAs<clang::IfStmt>("last");
  auto const* const parent = result.Nodes.getNodeAs<clang::IfStmt>("parent");
  bool const is_else_if{parent->getElse() == last};
  if (!is_else_if || last->getIfLoc().isMacroID()) {
    return;
  }
  diag(last->getIfLoc(),
       "the if ... else if chain ends without else; add one, with what is left "
       "to do or why there is nothing (CodingStyle T-24, MISRA C++:2023 "
       "9.4.1)");
}

}  // namespace house
