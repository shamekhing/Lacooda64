#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Operand/instruction validation and trace validation.

#include <cstddef>
#include <vector>

#include "Address.hpp"
#include "Control.hpp"
#include "Immediate.hpp"
#include "Instruction.hpp"
#include "Opcode.hpp"
#include "Operand.hpp"
#include "Operation.hpp"
#include "Register.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Validation
// -----------------------------------------------------------------------------

// Returns true when `o` is a well-formed operand of any supported kind.
[[nodiscard]] constexpr bool ValidOperand(Operand o) noexcept {
  switch (TagOf(o)) {
    case WordTag::kNone:
      return o == kNone;
    case WordTag::kAddress:
      return ValidAddress(o);
    case WordTag::kRegister:
      return RegisterBankOf(o) == RegisterBank::kValue ||
             RegisterBankOf(o) == RegisterBank::kAddress ||
             RegisterBankOf(o) == RegisterBank::kFlag;
    case WordTag::kImmediate:
      return true;
    case WordTag::kControl:
      return static_cast<Word>(ControlRegOf(o)) <
             static_cast<Word>(ControlReg::kCount);
    default:
      return false;
  }
}

// Returns true when instruction `i` has a well-formed operation word and its
// operands satisfy the per-opcode arity and writability rules.
[[nodiscard]] constexpr bool ValidInstruction(const Instruction& i) noexcept {
  if (!IsTag(Operation(i), WordTag::kOperation)) return false;
  if (!ValidOperand(Dst(i)) || !ValidOperand(Src0(i)) || !ValidOperand(Src1(i)))
    return false;

  switch (OpcodeOf(Operation(i))) {
    case Opcode::kNop:
    case Opcode::kHalt:
      // No operands expected.
      return IsNone(Dst(i)) && IsNone(Src0(i)) && IsNone(Src1(i));

    case Opcode::kSet:
    case Opcode::kCopy:
      // dst must be writable; src0 must be present.
      return IsWritable(Dst(i)) && !IsNone(Src0(i));

    case Opcode::kLoad:
      // dst is a register; src0 must be present.
      return IsRegister(Dst(i)) && !IsNone(Src0(i));

    case Opcode::kStore:
      // dst is a control/address-attribute; src0 must be present.
      return (IsControl(Dst(i)) ||
              (IsAddress(Dst(i)) && IsAttribute(Dst(i)))) &&
             !IsNone(Src0(i));

    case Opcode::kSwap:
      // Both operand slots must be present.
      return !IsNone(Dst(i)) && !IsNone(Src0(i));

    case Opcode::kSelect:
      return IsAddressRegister(Dst(i)) && IsAddress(Src0(i));

    case Opcode::kCount:
      return IsValueRegister(Dst(i)) && IsAddress(Src0(i));

    case Opcode::kMove:
      // Both must be whole objects (not attributes).
      return IsAddress(Dst(i)) && IsAddress(Src0(i)) && IsObject(Dst(i)) &&
             IsObject(Src0(i));

    case Opcode::kSummon:
      // Destination is a slot/card object; src0 is the card being summoned.
      return IsAddress(Dst(i)) && IsObject(Dst(i)) &&
             (LevelOf(Dst(i)) == AddressLevel::kSlot ||
              LevelOf(Dst(i)) == AddressLevel::kCard) &&
             !IsNone(Src0(i));

    case Opcode::kPosition:
    case Opcode::kNegate:
      return IsAddressSource(Dst(i));

    case Opcode::kEquip:
    case Opcode::kControl:
      return IsAddressSource(Dst(i)) && IsAddressSource(Src0(i));

    case Opcode::kCounter:
      return IsAddressSource(Dst(i)) && !IsNone(Src0(i));

    case Opcode::kRestrict:
      // dst must name an attribute, not a whole object.
      return IsAddress(Dst(i)) && IsAttribute(Dst(i)) && !IsNone(Src0(i));

    case Opcode::kAlu: {
      if (!IsValueRegister(Dst(i)) || !IsValueSource(Src0(i))) return false;
      switch (static_cast<AluOp>(SubcodeOf(Operation(i)))) {
        case AluOp::kAdd:
        case AluOp::kSubtract:
        case AluOp::kMultiply:
        case AluOp::kDivide:
        case AluOp::kModulo:
        case AluOp::kMinimum:
        case AluOp::kMaximum:
          return IsValueSource(Src1(i));
        case AluOp::kNegate:
        case AluOp::kAbsolute:
          return IsNone(Src1(i));
      }
      return false;
    }

    case Opcode::kCompare:
      // dst is a flag register; both sources must be present.
      return IsFlagRegister(Dst(i)) && !IsNone(Src0(i)) && !IsNone(Src1(i));

    case Opcode::kJump:
      // Unconditional: target is an immediate, no dst/src1.
      return IsNone(Dst(i)) && IsImmediate(Src0(i)) && IsNone(Src1(i));

    case Opcode::kJumpIf:
      // src0 is a flag, src1 is the immediate target.
      return IsNone(Dst(i)) && IsFlagRegister(Src0(i)) && IsImmediate(Src1(i));

    case Opcode::kDamage:
    case Opcode::kGainLp:
    case Opcode::kPayLp:
      return IsAddressSource(Dst(i)) && IsValueSource(Src0(i));

    case Opcode::kRandom:
      if (IsValueRegister(Dst(i))) return IsImmediate(Src0(i));
      if (IsAddressRegister(Dst(i))) return IsAddress(Src0(i));
      return false;

    case Opcode::kEvent:
      // An event must specify a non-kNone EventKind in its subcode.
      return SubcodeOf(Operation(i)) != Sub(EventKind::kNone);

    case Opcode::kChain:
      // Chain subcodes are bounded by ChainOp::kEnd.
      return SubcodeOf(Operation(i)) <= Sub(ChainOp::kEnd);
  }
  return false;
}

// A validated program is a flat sequence of instructions.
using Trace = std::vector<Instruction>;

// Trace-level validation errors reported by ValidateTrace().
enum class ValidationError : Word {
  kNone = 0,
  kInvalidInstruction,  // An instruction failed ValidInstruction().
  kJumpOutOfRange,      // A Jump/JumpIf target lies outside the trace.
};

// Outcome of ValidateTrace(): an error code plus the offending index.
struct ValidationResult {
  ValidationError error_{ValidationError::kNone};
  Word instruction_{0};  // Index (PC) of the offending instruction.

  [[nodiscard]] constexpr bool ok() const noexcept {
    return error_ == ValidationError::kNone;
  }
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return ok();
  }
};

// Validates a whole trace: every instruction must be well-formed and every
// Jump/JumpIf target must be a valid instruction index.
[[nodiscard]] inline ValidationResult ValidateTrace(
    const Trace& trace) noexcept {
  for (Word pc = 0; pc < trace.size(); ++pc) {
    const auto& i = trace[static_cast<std::size_t>(pc)];
    if (!ValidInstruction(i)) return {ValidationError::kInvalidInstruction, pc};

    const auto op = OpcodeOf(Operation(i));
    if (op == Opcode::kJump) {
      const SignedWord t = ImmediateValue(Src0(i));
      if (t < 0 || static_cast<Word>(t) >= trace.size())
        return {ValidationError::kJumpOutOfRange, pc};
    }
    if (op == Opcode::kJumpIf) {
      const SignedWord t = ImmediateValue(Src1(i));
      if (t < 0 || static_cast<Word>(t) >= trace.size())
        return {ValidationError::kJumpOutOfRange, pc};
    }
  }
  return {};
}

}  // namespace openjoey::lacooda64
