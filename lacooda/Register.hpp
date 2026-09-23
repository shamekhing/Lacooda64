#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Register word: packed [bank|index].

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Register word
//
// payload:
//   bits 9..8 bank
//   bits 7..0 index
// -----------------------------------------------------------------------------

// Three register banks, each with 256 entries (8-bit index). They pack into a
// single Register-tagged operand: low 8 bits = index, next 2 = bank.
enum class RegisterBank : Word {
  kValue = 0,    // General-purpose value registers (V0..V255).
  kAddress = 1,  // Address/pointer registers (A0..A255).
  kFlag = 2,     // Boolean flag registers (F0..F255), used as 0/1 words.
};

inline constexpr Word kRegisterIndexBits = 8;
inline constexpr Word kRegisterBankBits = 2;
inline constexpr Word kRegisterIndexMask = (Word{1} << kRegisterIndexBits) - 1;
inline constexpr Word kRegisterBankShift = kRegisterIndexBits;
inline constexpr Word kRegisterBankMask = (Word{1} << kRegisterBankBits) - 1;

// Builds a Register-tagged operand from a bank and an 8-bit index.
[[nodiscard]] constexpr Operand Reg(RegisterBank bank,
                                    RegisterIndex index) noexcept {
  return MakeTagged(
      WordTag::kRegister,
      ((static_cast<Word>(bank) & kRegisterBankMask) << kRegisterBankShift) |
          (index & kRegisterIndexMask));
}

// Convenience aliases for the three register banks (V/A/F prefix notation).
[[nodiscard]] constexpr Operand V(RegisterIndex i) noexcept {
  return Reg(RegisterBank::kValue, i);
}
[[nodiscard]] constexpr Operand A(RegisterIndex i) noexcept {
  return Reg(RegisterBank::kAddress, i);
}
[[nodiscard]] constexpr Operand F(RegisterIndex i) noexcept {
  return Reg(RegisterBank::kFlag, i);
}

// Decodes the register bank from a Register-tagged operand.
[[nodiscard]] constexpr RegisterBank RegisterBankOf(Operand r) noexcept {
  return static_cast<RegisterBank>((PayloadOf(r) >> kRegisterBankShift) &
                                   kRegisterBankMask);
}
// Decodes the 8-bit register index from a Register-tagged operand.
[[nodiscard]] constexpr RegisterIndex RegisterIndexOf(Operand r) noexcept {
  return PayloadOf(r) & kRegisterIndexMask;
}
// True when `r` is a register of any bank.
[[nodiscard]] constexpr bool IsRegister(Operand r) noexcept {
  return IsTag(r, WordTag::kRegister);
}

}  // namespace openjoey::lacooda64
