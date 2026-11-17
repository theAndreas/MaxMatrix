// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "operator_precedence_check.h"

#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/OperationKinds.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/OperatorPrecedence.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringRef.h"

namespace house {

namespace {

using Level = clang::prec::Level;

// The rank of a binary operator, by clang's levels, which order the operators
// as the ranks of MISRA C++:2023 Rule 8.0.1 do. Unknown for a pointer to
// member, which the rule ranks with the primary expressions.
Level RankOf(clang::BinaryOperatorKind opcode) {
  switch (opcode) {
    case clang::BO_Mul:
    case clang::BO_Div:
    case clang::BO_Rem:
      return clang::prec::Multiplicative;
    case clang::BO_Add:
    case clang::BO_Sub:
      return clang::prec::Additive;
    case clang::BO_Shl:
    case clang::BO_Shr:
      return clang::prec::Shift;
    case clang::BO_Cmp:
      return clang::prec::Spaceship;
    case clang::BO_LT:
    case clang::BO_GT:
    case clang::BO_LE:
    case clang::BO_GE:
      return clang::prec::Relational;
    case clang::BO_EQ:
    case clang::BO_NE:
      return clang::prec::Equality;
    case clang::BO_And:
      return clang::prec::And;
    case clang::BO_Xor:
      return clang::prec::ExclusiveOr;
    case clang::BO_Or:
      return clang::prec::InclusiveOr;
    case clang::BO_LAnd:
      return clang::prec::LogicalAnd;
    case clang::BO_LOr:
      return clang::prec::LogicalOr;
    case clang::BO_Comma:
      return clang::prec::Comma;
    case clang::BO_PtrMemD:
    case clang::BO_PtrMemI:
      return clang::prec::Unknown;
    default:
      return clang::prec::Assignment;
  }
}

// The rank of an expression: of its binary or conditional operator, built-in
// or overloaded, as the rule ranks the element at the root of the parse tree.
// Unknown for everything the rule ranks highest: a name, a call, a cast, a
// unary operator.
Level RankOf(clang::Expr const& expression) {
  if (auto const* const binary =
          llvm::dyn_cast<clang::BinaryOperator>(&expression)) {
    return RankOf(binary->getOpcode());
  }
  if (auto const* const overloaded =
          llvm::dyn_cast<clang::CXXOperatorCallExpr>(&expression)) {
    return overloaded->isInfixBinaryOp()
               ? RankOf(clang::BinaryOperator::getOverloadedOpcode(
                     overloaded->getOperator()))
               : clang::prec::Unknown;
  }
  if (llvm::isa<clang::ConditionalOperator>(expression)) {
    return clang::prec::Conditional;
  }
  if (llvm::isa<clang::CXXThrowExpr>(expression)) {
    return clang::prec::Comma;
  }
  return clang::prec::Unknown;
}

// Whether the rule asks anything of the operands of an expression of `rank`:
// not of the comma, `throw` or an assignment, nor of the highest rank.
bool RanksItsOperands(Level rank) { return rank > clang::prec::Assignment; }

// The spelling of an operator, for the message.
llvm::StringRef SpellingOf(clang::Expr const& expression) {
  if (auto const* const binary =
          llvm::dyn_cast<clang::BinaryOperator>(&expression)) {
    return binary->getOpcodeStr();
  }
  if (auto const* const overloaded =
          llvm::dyn_cast<clang::CXXOperatorCallExpr>(&expression)) {
    return clang::BinaryOperator::getOpcodeStr(
        clang::BinaryOperator::getOverloadedOpcode(overloaded->getOperator()));
  }
  return "?:";
}

// Where an operator is spelled, which names it in a report.
clang::SourceLocation OperatorLocation(clang::Expr const& expression) {
  if (auto const* const binary =
          llvm::dyn_cast<clang::BinaryOperator>(&expression)) {
    return binary->getOperatorLoc();
  }
  if (auto const* const overloaded =
          llvm::dyn_cast<clang::CXXOperatorCallExpr>(&expression)) {
    return overloaded->getOperatorLoc();
  }
  return llvm::cast<clang::ConditionalOperator>(expression).getQuestionLoc();
}

// An operand as written: without the conversions, temporaries and copies the
// compiler adds around it, as an overloaded operator that takes its operands
// by value has them; a parenthesis stays, which IgnoreUnlessSpelledInSource()
// would take away as well.
clang::Expr const* AsWritten(clang::Expr const* operand) {
  clang::Expr const* written{operand->IgnoreImplicit()};
  auto const* construct{llvm::dyn_cast<clang::CXXConstructExpr>(written)};
  while (construct != nullptr && construct->getNumArgs() == 1 &&
         construct->getParenOrBraceRange().isInvalid()) {
    written = construct->getArg(0)->IgnoreImplicit();
    construct = llvm::dyn_cast<clang::CXXConstructExpr>(written);
  }
  return written;
}

// The operands of an operator as written.
llvm::SmallVector<clang::Expr const*, 3> OperandsOf(
    clang::Expr const& expression) {
  llvm::SmallVector<clang::Expr const*, 3> operands;
  if (auto const* const binary =
          llvm::dyn_cast<clang::BinaryOperator>(&expression)) {
    operands.push_back(binary->getLHS());
    operands.push_back(binary->getRHS());
  } else if (auto const* const overloaded =
                 llvm::dyn_cast<clang::CXXOperatorCallExpr>(&expression)) {
    operands.push_back(overloaded->getArg(0));
    operands.push_back(overloaded->getArg(1));
  } else {
    auto const& conditional =
        llvm::cast<clang::ConditionalOperator>(expression);
    operands.push_back(conditional.getCond());
    operands.push_back(conditional.getTrueExpr());
    operands.push_back(conditional.getFalseExpr());
  }
  for (clang::Expr const*& operand : operands) {
    operand = AsWritten(operand);
  }
  return operands;
}

// Whether `location` is written in the file being checked, or in the argument
// of a macro, which the fix-it can change there as well; not in the body of a
// macro, which every expansion shares.
bool IsWrittenHere(clang::SourceLocation location,
                   clang::SourceManager const& manager) {
  return location.isFileID() || manager.isMacroArgExpansion(location);
}

}  // namespace

void OperatorPrecedenceCheck::registerMatchers(
    clang::ast_matchers::MatchFinder* finder) {
  using clang::ast_matchers::anyOf;
  using clang::ast_matchers::binaryOperator;
  using clang::ast_matchers::conditionalOperator;
  using clang::ast_matchers::cxxOperatorCallExpr;
  using clang::ast_matchers::expr;
  using clang::ast_matchers::isInTemplateInstantiation;
  using clang::ast_matchers::unaryExprOrTypeTraitExpr;
  using clang::ast_matchers::unless;
  // As written, not in each instantiation of a template.
  finder->addMatcher(expr(anyOf(binaryOperator(), cxxOperatorCallExpr(),
                                conditionalOperator()),
                          unless(isInTemplateInstantiation()))
                         .bind("operator"),
                     this);
  finder->addMatcher(
      unaryExprOrTypeTraitExpr(unless(isInTemplateInstantiation()))
          .bind("sizeof"),
      this);
}

void OperatorPrecedenceCheck::check(
    clang::ast_matchers::MatchFinder::MatchResult const& result) {
  clang::SourceManager const& manager = *result.SourceManager;
  if (auto const* const size =
          result.Nodes.getNodeAs<clang::UnaryExprOrTypeTraitExpr>("sizeof")) {
    CheckSizeof(*size, manager);
    return;
  }
  auto const* const parent = result.Nodes.getNodeAs<clang::Expr>("operator");
  Level const parent_rank{RankOf(*parent)};
  bool const in_body_of_macro{
      !IsWrittenHere(OperatorLocation(*parent), manager)};
  if (!RanksItsOperands(parent_rank) || in_body_of_macro) {
    return;
  }
  for (clang::Expr const* const operand : OperandsOf(*parent)) {
    Level const rank{RankOf(*operand)};
    if (rank != clang::prec::Unknown && rank > parent_rank) {
      Parenthesize(*operand, *parent, manager);
    }
  }
}

void OperatorPrecedenceCheck::Parenthesize(
    clang::Expr const& operand, clang::Expr const& parent,
    clang::SourceManager const& manager) {
  clang::CharSourceRange const range{clang::Lexer::makeFileCharRange(
      clang::CharSourceRange::getTokenRange(operand.getSourceRange()), manager,
      getLangOpts())};
  if (range.isInvalid()) {
    return;
  }
  diag(range.getBegin(),
       "'%0' as an operand of '%1' depends on the precedence of operators: "
       "put it in parentheses (CodingStyle T-23, MISRA C++:2023 8.0.1)")
      << SpellingOf(operand) << SpellingOf(parent)
      << clang::FixItHint::CreateInsertion(range.getBegin(), "(")
      << clang::FixItHint::CreateInsertion(range.getEnd(), ")");
}

void OperatorPrecedenceCheck::CheckSizeof(
    clang::UnaryExprOrTypeTraitExpr const& size,
    clang::SourceManager const& manager) {
  bool const of_an_expression{size.getKind() == clang::UETT_SizeOf &&
                              !size.isArgumentType()};
  if (!of_an_expression ||
      llvm::isa<clang::ParenExpr>(size.getArgumentExpr()) ||
      !IsWrittenHere(size.getOperatorLoc(), manager)) {
    return;
  }
  clang::CharSourceRange const range{clang::Lexer::makeFileCharRange(
      clang::CharSourceRange::getTokenRange(
          size.getArgumentExpr()->getSourceRange()),
      manager, getLangOpts())};
  if (range.isInvalid()) {
    return;
  }
  diag(range.getBegin(),
       "the operand of 'sizeof' stands without parentheses: put it in "
       "parentheses (CodingStyle T-23, MISRA C++:2023 8.0.1)")
      << clang::FixItHint::CreateInsertion(range.getBegin(), "(")
      << clang::FixItHint::CreateInsertion(range.getEnd(), ")");
}

}  // namespace house
