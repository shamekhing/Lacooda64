#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Operand classification predicates.

#include "Address.hpp"
#include "Control.hpp"
#include "Immediate.hpp"
#include "Register.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Operand classification
// -----------------------------------------------------------------------------

// True when `o` is the "no operand" sentinel (a zero WordTag::kNone word).
[[nodiscard]] constexpr bool IsNone(Operand o) noexcept {
  return TagOf(o) == WordTag::kNone;
}

// True when `o` is a Value-register operand.
[[nodiscard]] constexpr bool IsValueRegister(Operand o) noexcept {
  return IsRegister(o) && RegisterBankOf(o) == RegisterBank::kValue;
}
// True when `o` is an Address-register operand.
[[nodiscard]] constexpr bool IsAddressRegister(Operand o) noexcept {
  return IsRegister(o) && RegisterBankOf(o) == RegisterBank::kAddress;
}
// True when `o` is a Flag-register operand.
[[nodiscard]] constexpr bool IsFlagRegister(Operand o) noexcept {
  return IsRegister(o) && RegisterBankOf(o) == RegisterBank::kFlag;
}

// True when `o` refers to a duel location by value or by an address register.
[[nodiscard]] constexpr bool IsAddressSource(Operand o) noexcept {
  return IsAddress(o) || IsAddressRegister(o);
}

// True when `o` can supply a value: immediates, control regs, value/flag regs,
// and addresses that point at an attribute (not a whole object).
[[nodiscard]] constexpr bool IsValueSource(Operand o) noexcept {
  return IsImmediate(o) || IsControl(o) || IsValueRegister(o) ||
         IsFlagRegister(o) || (IsAddress(o) && IsAttribute(o));
}

// True when `o` is a valid destination to write a value into: a register, a
// control register, or an address that names an attribute slot.
[[nodiscard]] constexpr bool IsWritable(Operand o) noexcept {
  return IsRegister(o) || IsControl(o) || (IsAddress(o) && IsAttribute(o));
}

}  // namespace openjoey::lacooda64
