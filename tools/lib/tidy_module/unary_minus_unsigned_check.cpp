// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "unary_minus_unsigned_check.h"

#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

namespace house {

namespace {

// Whether `type` is a character, which the rule ranks apart from the numbers:
// plain char and the wide ones, not unsigned char, which uint8_t is.
// isAnyCharacterType() and isCharType() take unsigned char as well.
bool IsCharacter(clang::QualType type) {
  bool const is_plain_char{
      type->isSpecificBuiltinType(clang::BuiltinType::Char_U) ||
      type->isSpecificBuiltinType(clang::BuiltinType::Char_S)};
  return is_plain_char || type->isWideCharType() || type->isChar8Type() ||
         type->isChar16Type() || type->isChar32Type();
}

// Whether the rule counts `type` as unsigned: an unsigned integer, an unscoped
// enum of one as well, but not bool and not a character.
bool IsUnsignedNumber(clang::QualType type) {
  return type->isUnsignedIntegerType() && !type->isBooleanType() &&
         !IsCharacter(type);
}

}  // namespace

void UnaryMinusUnsignedCheck::registerMatchers(
    clang::ast_matchers::MatchFinder* finder) {
  using clang::ast_matchers::hasOperatorName;
  using clang::ast_matchers::unaryOperator;
  finder->addMatcher(unaryOperator(hasOperatorName("-")).bind("minus"), this);
}

void UnaryMinusUnsignedCheck::check(
    clang::ast_matchers::MatchFinder::MatchResult const& result) {
  auto const* const minus =
      result.Nodes.getNodeAs<clang::UnaryOperator>("minus");
  // The type before the promotion, as the rule asks: an implicit cast of a
  // uint8_t to int lies between the operator and the operand.
  clang::QualType const type{
      minus->getSubExpr()->IgnoreParenImpCasts()->getType()};
  if (type->isDependentType() || !IsUnsignedNumber(type)) {
    return;
  }
  diag(minus->getOperatorLoc(),
       "unary minus on '%0', an unsigned type: the result is unsigned, or "
       "after the promotion signed, by the width of the type; make the "
       "operand signed first (CodingStyle T-25, MISRA C++:2023 8.3.1)")
      << type.getUnqualifiedType().getAsString();
}

}  // namespace house
