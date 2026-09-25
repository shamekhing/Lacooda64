#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Register word: consumed low-to-high as bank, index.

#include <cstddef>
#include <limits>

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Register word
//
// Payload bits (10 used, 50 reserved):
//
//   payload bit (after removing the low tag with PayloadOf)
//   59..10                          9..2   1..0
//   +------------------------------+------+--------+
//   | reserved                     | index| bank   |
//   +------------------------------+------+--------+
//                  50                  8       2
//
// Full 64-bit word:
//   3..0 = WordTag::kRegister
//
// Banks: 0 = Value (V), 1 = Address (A), 2 = Flag (F).
// Each bank has indices 0..255; bank 3 is invalid.
// Reg masks the bank/index fields and emits zero reserved bits.
// -----------------------------------------------------------------------------

// Three register banks, each with 256 entries (8-bit index). They pack into a
// single Register-tagged operand: low 2 payload bits = bank, next 8 = index.
enum class RegisterBank : Word {
  kValue = 0,    // General-purpose value registers (V0..V255).
  kAddress = 1,  // Address/pointer registers (A0..A255).
  kFlag = 2,     // Boolean flag registers (F0..F255), used as 0/1 words.
};

inline constexpr Word kRegisterIndexBits = 8;
inline constexpr Word kRegisterBankBits = 2;
static_assert(kRegisterBankBits > 0 && kRegisterIndexBits > 0 && kRegisterBankBits + kRegisterIndexBits <= kPayloadBits, "Register fields must fit the payload");
static_assert(kRegisterIndexBits < std::numeric_limits<std::size_t>::digits, "Register count must fit size_t");
inline constexpr std::size_t kRegisterCount = std::size_t{1} << kRegisterIndexBits;
inline constexpr Word kRegisterIndexMask = (Word{1} << kRegisterIndexBits) - 1;
inline constexpr Word kRegisterBankShift = 0;
inline constexpr Word kRegisterIndexShift = kRegisterBankShift + kRegisterBankBits;
inline constexpr Word kRegisterBankMask = (Word{1} << kRegisterBankBits) - 1;

static_assert(static_cast<Word>(RegisterBank::kFlag) <= kRegisterBankMask, "Bank width cannot represent all register banks");

struct RegisterFields {
  RegisterBank bank;
  RegisterIndex index;
};

// Consume bank then index. Assumes a register word.
[[nodiscard]] constexpr RegisterFields DecodeRegister(Operand reg) noexcept {
  Word cursor = PayloadOf(reg);
  const auto bank = static_cast<RegisterBank>(TakeField<kRegisterBankBits>(cursor));
  const auto index = TakeField<kRegisterIndexBits>(cursor);
  return {bank, index};
}

// Builds a Register-tagged operand from a bank and an 8-bit index.
[[nodiscard]] constexpr Operand Reg(RegisterBank bank, RegisterIndex index) noexcept { return MakeTagged(WordTag::kRegister, ((static_cast<Word>(bank) & kRegisterBankMask) << kRegisterBankShift) | ((index & kRegisterIndexMask) << kRegisterIndexShift)); }

// Convenience aliases for the three register banks (V/A/F prefix notation).
[[nodiscard]] constexpr Operand V(RegisterIndex i) noexcept { return Reg(RegisterBank::kValue, i); }
[[nodiscard]] constexpr Operand A(RegisterIndex i) noexcept { return Reg(RegisterBank::kAddress, i); }
[[nodiscard]] constexpr Operand F(RegisterIndex i) noexcept { return Reg(RegisterBank::kFlag, i); }

// Decodes the register bank from a Register-tagged operand.
[[nodiscard]] constexpr RegisterBank RegisterBankOf(Operand r) noexcept { return static_cast<RegisterBank>((PayloadOf(r) >> kRegisterBankShift) & kRegisterBankMask); }
// Decodes the 8-bit register index from a Register-tagged operand.
[[nodiscard]] constexpr RegisterIndex RegisterIndexOf(Operand r) noexcept { return (PayloadOf(r) >> kRegisterIndexShift) & kRegisterIndexMask; }
// True when `r` is a register of any bank.
[[nodiscard]] constexpr bool IsRegister(Operand r) noexcept { return IsTag(r, WordTag::kRegister); }

}  // namespace openjoey::lacooda64
