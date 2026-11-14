// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_TIDY_MODULE_BITWISE_IN_BIT_CLASS_CHECK_H_
#define TOOLS_TIDY_MODULE_BITWISE_IN_BIT_CLASS_CHECK_H_

#include <string>
#include <vector>

#include "clang-tidy/ClangTidyCheck.h"

namespace house {

/*!
 * \brief house-bitwise-in-bit-class: the bitwise operators `&`, `|`, `^`, `~`,
 *        `<<` and `>>`, and `&=` and its like, stand only in the bit class,
 *        CodingStyle H-9.
 *
 * Everywhere else a bit is read and written through the class's functions,
 * `Bit<uint8_t>::SetBits(DDRB, _BV(DDB5))`, not `DDRB |= _BV(DDB5)`: the class
 * holds once what such an expression gets wrong, the promotion to `int`, a
 * shift as wide as the type, the read-modify-write of a register.
 *
 * Not reported:
 *
 * - An operator of a macro of a system header, as the shift of avr-libc's
 *   `_BV()`: the platform's names for its bits.
 * - A `|` of named masks, constants or a system header's macros, in the
 *   initializer of a `constexpr` variable: `constexpr uint8_t kFault{kHot |
 *   kLow};`.
 * - An overloaded operator, as those of `std::byte`, nor the builtin one in its
 *   body, where the operator is a type's own, which says what its bits mean:
 *   `&`, `|` or `^` of two of the type, `~` of one, a shift of one by a count
 *   that gives the type back by value, or the `=` form of any of them. The
 *   body of a stream's `<<`, which gives its stream back by reference, is
 *   read as any other.
 *
 * In a template, the operator is reported in an instantiation, where the types
 * of its operands are known.
 *
 * Option `BitClasses`: the names of the bit classes, separated by `;`, `Bit` by
 * default, which matches a class `Bit` in any namespace.
 */
class BitwiseInBitClassCheck : public clang::tidy::ClangTidyCheck {
 public:
  BitwiseInBitClassCheck(llvm::StringRef name,
                         clang::tidy::ClangTidyContext* context);

  bool isLanguageVersionSupported(
      clang::LangOptions const& lang_opts) const override {
    return lang_opts.CPlusPlus;
  }
  void storeOptions(clang::tidy::ClangTidyOptions::OptionMap& options) override;
  void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
  void check(
      clang::ast_matchers::MatchFinder::MatchResult const& result) override;

 private:
  std::string bit_classes_;
  std::vector<llvm::StringRef> names_;
};

}  // namespace house

#endif  // TOOLS_TIDY_MODULE_BITWISE_IN_BIT_CLASS_CHECK_H_
