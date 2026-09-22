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
    Value   = 0, // General-purpose value registers (V0..V255).
    Address = 1, // Address/pointer registers (A0..A255).
    Flag    = 2, // Boolean flag registers (F0..F255), used as 0/1 words.
};

inline constexpr Word REGISTER_INDEX_BITS = 8;
inline constexpr Word REGISTER_BANK_BITS  = 2;
inline constexpr Word REGISTER_INDEX_MASK = (Word{1} << REGISTER_INDEX_BITS) - 1;
inline constexpr Word REGISTER_BANK_SHIFT = REGISTER_INDEX_BITS;
inline constexpr Word REGISTER_BANK_MASK  = (Word{1} << REGISTER_BANK_BITS) - 1;

// Builds a Register-tagged operand from a bank and an 8-bit register index.
[[nodiscard]] constexpr Operand Reg(RegisterBank bank, RegisterIndex index) noexcept {
    return makeTagged(
        WordTag::Register,
        ((static_cast<Word>(bank) & REGISTER_BANK_MASK) << REGISTER_BANK_SHIFT) |
        (index & REGISTER_INDEX_MASK)
    );
}

// Convenience aliases for the three register banks (V/A/F prefix notation).
[[nodiscard]] constexpr Operand V(RegisterIndex i) noexcept {
    return Reg(RegisterBank::Value, i);
}
[[nodiscard]] constexpr Operand A(RegisterIndex i) noexcept {
    return Reg(RegisterBank::Address, i);
}
[[nodiscard]] constexpr Operand F(RegisterIndex i) noexcept {
    return Reg(RegisterBank::Flag, i);
}

// Decodes the register bank from a Register-tagged operand.
[[nodiscard]] constexpr RegisterBank registerBank(Operand r) noexcept {
    return static_cast<RegisterBank>(
        (payloadOf(r) >> REGISTER_BANK_SHIFT) & REGISTER_BANK_MASK
    );
}
// Decodes the 8-bit register index from a Register-tagged operand.
[[nodiscard]] constexpr RegisterIndex registerIndex(Operand r) noexcept {
    return payloadOf(r) & REGISTER_INDEX_MASK;
}
// True when `r` is a register of any bank.
[[nodiscard]] constexpr bool isRegister(Operand r) noexcept {
    return isTag(r, WordTag::Register);
}

} // namespace openjoey::lacooda64
