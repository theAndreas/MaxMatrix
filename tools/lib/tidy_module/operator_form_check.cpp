// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "operator_form_check.h"

#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/OperatorKinds.h"
#include "clang/Basic/SourceManager.h"

namespace house {
namespace {

using clang::FunctionDecl;
using clang::OverloadedOperatorKind;

// The form CodingStyle C-15 gives an operator.
enum class Form {
  kMember,        // changes its operand, or unary
  kHiddenFriend,  // binary, leaves its operands as they are
  kAny,           // a member by the language, or not ruled
};

Form FormOf(OverloadedOperatorKind kind, unsigned operands) {
  switch (kind) {
    case clang::OO_PlusEqual:
    case clang::OO_MinusEqual:
    case clang::OO_StarEqual:
    case clang::OO_SlashEqual:
    case clang::OO_PercentEqual:
    case clang::OO_CaretEqual:
    case clang::OO_AmpEqual:
    case clang::OO_PipeEqual:
    case clang::OO_LessLessEqual:
    case clang::OO_GreaterGreaterEqual:
    case clang::OO_PlusPlus:
    case clang::OO_MinusMinus:
    case clang::OO_Tilde:
    case clang::OO_Exclaim:
      return Form::kMember;
    case clang::OO_Plus:
    case clang::OO_Minus:
    case clang::OO_Star:
    case clang::OO_Amp:
      // Unary as -x, *x, &x; binary as a - b.
      return operands == 1 ? Form::kMember : Form::kHiddenFriend;
    case clang::OO_Slash:
    case clang::OO_Percent:
    case clang::OO_Caret:
    case clang::OO_Pipe:
    case clang::OO_Less:
    case clang::OO_Greater:
    case clang::OO_LessEqual:
    case clang::OO_GreaterEqual:
    case clang::OO_EqualEqual:
    case clang::OO_ExclaimEqual:
    case clang::OO_Spaceship:
    case clang::OO_LessLess:
    case clang::OO_GreaterGreater:
    case clang::OO_AmpAmp:
    case clang::OO_PipePipe:
      return Form::kHiddenFriend;
    default:
      return Form::kAny;
  }
}

// The class a parameter of type `type` takes, by value or by reference, also
// a specialization of a class template as written in a template; else null.
clang::CXXRecordDecl const* ClassOf(clang::QualType type) {
  type = type.getNonReferenceType();
  if (auto const* const record = type->getAsCXXRecordDecl()) {
    return record;
  }
  if (auto const* const specialization =
          type->getAs<clang::TemplateSpecializationType>()) {
    if (auto const* const class_template =
            llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
                specialization->getTemplateName().getAsTemplateDecl())) {
      return class_template->getTemplatedDecl();
    }
  }
  return nullptr;
}

// Whether `record` is declared in the same file as `function`: a class whose
// friend the operator can be.
bool InSameFile(clang::CXXRecordDecl const* record,
                FunctionDecl const& function,
                clang::SourceManager const& sources) {
  if (record == nullptr) {
    return false;
  }
  clang::FileID const record_file{
      sources.getFileID(sources.getExpansionLoc(record->getLocation()))};
  clang::FileID const function_file{
      sources.getFileID(sources.getExpansionLoc(function.getLocation()))};
  return record_file == function_file;
}

}  // namespace

void OperatorFormCheck::registerMatchers(
    clang::ast_matchers::MatchFinder* finder) {
  using clang::ast_matchers::functionDecl;
  using clang::ast_matchers::isImplicit;
  using clang::ast_matchers::isInstantiated;
  using clang::ast_matchers::unless;
  // As written, not in each instantiation of a template.
  finder->addMatcher(
      functionDecl(unless(isImplicit()), unless(isInstantiated()))
          .bind("function"),
      this);
}

void OperatorFormCheck::check(
    clang::ast_matchers::MatchFinder::MatchResult const& result) {
  auto const* const function = result.Nodes.getNodeAs<FunctionDecl>("function");
  if (!function->isOverloadedOperator() || !function->isFirstDecl()) {
    return;
  }
  auto const* const method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
  bool const member = method != nullptr && method->isInstance();
  unsigned const operands = function->getNumParams() + (member ? 1U : 0U);
  Form const form{FormOf(function->getOverloadedOperator(), operands)};
  // A friend in any of its declarations, as one declared in the class and
  // defined outside it.
  bool is_friend = false;
  for (FunctionDecl const* const decl : function->redecls()) {
    is_friend =
        is_friend || decl->getFriendObjectKind() != clang::Decl::FOK_None;
  }
  clang::SourceManager const& sources = *result.SourceManager;
  std::string const name{function->getNameAsString()};

  if (form == Form::kHiddenFriend) {
    bool free_for_own_class = false;
    if (!member && !is_friend) {
      for (clang::ParmVarDecl const* const param : function->parameters()) {
        free_for_own_class =
            free_for_own_class ||
            InSameFile(ClassOf(param->getType()), *function, sources);
      }
    }
    if (member || free_for_own_class) {
      diag(function->getLocation(),
           "make '%0' a hidden friend: a friend defined in the class, with "
           "both operands as its parameters (CodingStyle C-15)")
          << name;
    }
  } else if (form == Form::kMember && !member) {
    bool const own_class =
        is_friend || (function->getNumParams() > 0 &&
                      InSameFile(ClassOf(function->getParamDecl(0)->getType()),
                                 *function, sources));
    if (own_class) {
      diag(function->getLocation(),
           "make '%0' a member: it changes its operand, or has only one "
           "(CodingStyle C-15)")
          << name;
    }
  }
}

}  // namespace house
