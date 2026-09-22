#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Operand classification predicates.

#include "Word.hpp"
#include "Address.hpp"
#include "Register.hpp"
#include "Immediate.hpp"
#include "Control.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Operand classification
// -----------------------------------------------------------------------------

// True when `o` is the "no operand" sentinel (a zero WordTag::None word).
[[nodiscard]] constexpr bool isNone(Operand o) noexcept {
    return tagOf(o) == WordTag::None;
}

// True when `o` is a Value-register operand.
[[nodiscard]] constexpr bool isValueRegister(Operand o) noexcept {
    return isRegister(o) && registerBank(o) == RegisterBank::Value;
}
// True when `o` is an Address-register operand.
[[nodiscard]] constexpr bool isAddressRegister(Operand o) noexcept {
    return isRegister(o) && registerBank(o) == RegisterBank::Address;
}
// True when `o` is a Flag-register operand.
[[nodiscard]] constexpr bool isFlagRegister(Operand o) noexcept {
    return isRegister(o) && registerBank(o) == RegisterBank::Flag;
}

// True when `o` refers to a duel location by value or by an address register.
[[nodiscard]] constexpr bool isAddressSource(Operand o) noexcept {
    return isAddress(o) || isAddressRegister(o);
}

// True when `o` can supply a value: immediates, control regs, value/flag regs,
// and addresses that point at an attribute (not a whole object).
[[nodiscard]] constexpr bool isValueSource(Operand o) noexcept {
    return isImmediate(o) || isControl(o) || isValueRegister(o) ||
           isFlagRegister(o) || (isAddress(o) && isAttribute(o));
}

// True when `o` is a valid destination to write a value into: a register, a
// control register, or an address that names an attribute slot.
[[nodiscard]] constexpr bool isWritable(Operand o) noexcept {
    return isRegister(o) || isControl(o) || (isAddress(o) && isAttribute(o));
}

} // namespace openjoey::lacooda64
