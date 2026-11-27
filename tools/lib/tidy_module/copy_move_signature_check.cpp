// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "copy_move_signature_check.h"

#include <optional>
#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/ExceptionSpecificationType.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Token.h"
#include "llvm/Support/Casting.h"

namespace house {
namespace {

using clang::CXXMethodDecl;
using clang::SourceLocation;

enum class Kind { kCopyConstructor, kMoveConstructor, kCopyAssign, kMoveAssign };

Kind KindOf(CXXMethodDecl const& member) {
  if (auto const* constructor{
          llvm::dyn_cast<clang::CXXConstructorDecl>(&member)}) {
    return constructor->isCopyConstructor() ? Kind::kCopyConstructor
                                            : Kind::kMoveConstructor;
  }
  return member.isCopyAssignmentOperator() ? Kind::kCopyAssign
                                           : Kind::kMoveAssign;
}

bool IsMove(Kind kind) {
  return (kind == Kind::kMoveConstructor) || (kind == Kind::kMoveAssign);
}

bool IsAssignment(Kind kind) {
  return (kind == Kind::kCopyAssign) || (kind == Kind::kMoveAssign);
}

char const* NameOf(Kind kind) {
  switch (kind) {
    case Kind::kCopyConstructor:
      return "copy constructor";
    case Kind::kMoveConstructor:
      return "move constructor";
    case Kind::kCopyAssign:
      return "copy assignment operator";
    case Kind::kMoveAssign:
      return "move assignment operator";
  }
  return "";
}

// The signature MISRA C++:2023 15.0.2 gives, for the message.
std::string SignatureOf(Kind kind, std::string const& name) {
  switch (kind) {
    case Kind::kCopyConstructor:
      return name + "(" + name + " const&)";
    case Kind::kMoveConstructor:
      return name + "(" + name + "&&) noexcept";
    case Kind::kCopyAssign:
      return name + "& operator=(" + name + " const&) &";
    case Kind::kMoveAssign:
      return name + "& operator=(" + name + "&&) & noexcept";
  }
  return "";
}

// Whether the exception specification is a noexcept that holds, or one that
// depends on a template and is left to its instantiation.
bool IsNoexcept(clang::FunctionProtoType const& type) {
  switch (type.getExceptionSpecType()) {
    case clang::EST_BasicNoexcept:
    case clang::EST_NoexceptTrue:
    case clang::EST_DependentNoexcept:
      return true;
    default:
      return false;
  }
}

// What is wrong with the parameter, or nothing: a copy takes `X const&`, a
// move `X&&`.
char const* ParameterFault(Kind kind, clang::QualType parameter) {
  if (IsMove(kind)) {
    bool const is_const{parameter->isRValueReferenceType() &&
                        parameter->getPointeeType().isConstQualified()};
    return is_const ? "taking a const rvalue reference" : nullptr;
  }
  if (!parameter->isReferenceType()) {
    return "taking its parameter by value";
  }
  clang::QualType const referred{parameter->getPointeeType()};
  bool const is_const_only{referred.isConstQualified() &&
                           !referred.isVolatileQualified()};
  return is_const_only ? nullptr : "taking a reference to other than const";
}

// What the check found on one function.
struct Faults {
  bool lacks_qualifier{false};  // an assignment without `&`
  bool rvalue_qualifier{false};  // an assignment with `&&`
  bool lacks_noexcept{false};    // a move without any exception specification
  bool other_noexcept{false};    // a move with noexcept(false) or throw()
  char const* parameter{nullptr};

  bool Any() const {
    return lacks_qualifier || rvalue_qualifier || lacks_noexcept ||
           other_noexcept || (parameter != nullptr);
  }
  // Whether a fix-it can mend all of it: the qualifier and the noexcept only.
  bool OnlyAdditions() const {
    return !rvalue_qualifier && !other_noexcept && (parameter == nullptr);
  }
  std::string Text() const {
    std::string text;
    auto const add{[&text](char const* part) {
      text += text.empty() ? "" : " and ";
      text += part;
    }};
    if (lacks_qualifier) {
      add("without the ref-qualifier '&', which lets it assign to a "
          "temporary");
    }
    if (rvalue_qualifier) {
      add("with the ref-qualifier '&&'");
    }
    if (lacks_noexcept) {
      add("without 'noexcept'");
    }
    if (other_noexcept) {
      add("with an exception specification other than 'noexcept'");
    }
    if (parameter != nullptr) {
      add(parameter);
    }
    return text;
  }
};

// Where the fix-its insert, or nothing where the lexer cannot tell.
std::optional<clang::FixItHint> FixOf(
    CXXMethodDecl const& member, Faults const& faults,
    clang::SourceManager const& manager, clang::LangOptions const& lang) {
  clang::FunctionTypeLoc const loc{member.getFunctionTypeLoc()};
  SourceLocation const paren{loc ? loc.getRParenLoc() : SourceLocation{}};
  if (paren.isInvalid() || paren.isMacroID()) {
    return std::nullopt;
  }
  SourceLocation const after_paren{
      clang::Lexer::getLocForEndOfToken(paren, 0, manager, lang)};
  if (faults.lacks_qualifier) {
    return clang::FixItHint::CreateInsertion(
        after_paren, faults.lacks_noexcept ? " & noexcept" : " &");
  }
  if (!llvm::isa<clang::CXXConstructorDecl>(member)) {
    // A move assignment with its `&`: the noexcept goes after it.
    std::optional<clang::Token> const amp{
        clang::Lexer::findNextToken(paren, manager, lang)};
    if (!amp || !amp->is(clang::tok::amp)) {
      return std::nullopt;
    }
    return clang::FixItHint::CreateInsertion(
        clang::Lexer::getLocForEndOfToken(amp->getLocation(), 0, manager,
                                          lang),
        " noexcept");
  }
  return clang::FixItHint::CreateInsertion(after_paren, " noexcept");
}

}  // namespace

void CopyMoveSignatureCheck::registerMatchers(
    clang::ast_matchers::MatchFinder* finder) {
  using clang::ast_matchers::anyOf;
  using clang::ast_matchers::cxxConstructorDecl;
  using clang::ast_matchers::cxxMethodDecl;
  using clang::ast_matchers::isCopyAssignmentOperator;
  using clang::ast_matchers::isCopyConstructor;
  using clang::ast_matchers::isInstantiated;
  using clang::ast_matchers::isMoveAssignmentOperator;
  using clang::ast_matchers::isMoveConstructor;
  using clang::ast_matchers::isUserProvided;
  using clang::ast_matchers::unless;
  // Not in an instantiation: a class template is judged once, at its
  // definition, where its signatures are written.
  finder->addMatcher(
      cxxMethodDecl(isUserProvided(), unless(isInstantiated()),
                    anyOf(cxxConstructorDecl(
                              anyOf(isCopyConstructor(), isMoveConstructor())),
                          isCopyAssignmentOperator(),
                          isMoveAssignmentOperator()))
          .bind("member"),
      this);
}

void CopyMoveSignatureCheck::check(
    clang::ast_matchers::MatchFinder::MatchResult const& result) {
  auto const* const member{result.Nodes.getNodeAs<CXXMethodDecl>("member")};
  auto const* const type{
      member->getType()->getAs<clang::FunctionProtoType>()};
  // Once, where it is first declared; a definition outside the class follows.
  if (!member->isFirstDecl() || (type == nullptr) ||
      (member->getNumParams() == 0)) {
    return;
  }
  Kind const kind{KindOf(*member)};
  clang::RefQualifierKind const qualifier{member->getRefQualifier()};
  Faults faults;
  faults.lacks_qualifier = IsAssignment(kind) && (qualifier == clang::RQ_None);
  faults.rvalue_qualifier =
      IsAssignment(kind) && (qualifier == clang::RQ_RValue);
  faults.lacks_noexcept =
      IsMove(kind) && (type->getExceptionSpecType() == clang::EST_None);
  faults.other_noexcept =
      IsMove(kind) && !faults.lacks_noexcept && !IsNoexcept(*type);
  faults.parameter = ParameterFault(kind, member->getParamDecl(0)->getType());
  if (!faults.Any()) {
    return;
  }
  // A fix-it only where the function is declared once: a second declaration
  // would keep the old signature and no longer match.
  bool const declared_once{member->isThisDeclarationADefinition() &&
                           (member->getMostRecentDecl() == member)};
  std::optional<clang::FixItHint> fix;
  if (faults.OnlyAdditions() && declared_once) {
    fix = FixOf(*member, faults, *result.SourceManager, getLangOpts());
  }
  std::string const name{member->getParent()->getName().str()};
  if (fix) {
    diag(member->getLocation(),
         "%0 of '%1' %2 (CodingStyle C-17, MISRA C++:2023 15.0.2)")
        << NameOf(kind) << name << faults.Text() << *fix;
    return;
  }
  diag(member->getLocation(),
       "%0 of '%1' %2; write it as '%3' (CodingStyle C-17, MISRA C++:2023 "
       "15.0.2)")
      << NameOf(kind) << name << faults.Text() << SignatureOf(kind, name);
}

}  // namespace house
