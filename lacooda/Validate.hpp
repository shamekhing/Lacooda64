#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Operand/instruction validation and trace validation.

#include <cstddef>
#include <vector>

#include "Word.hpp"
#include "Address.hpp"
#include "Register.hpp"
#include "Immediate.hpp"
#include "Control.hpp"
#include "Opcode.hpp"
#include "Operation.hpp"
#include "Instruction.hpp"
#include "Operand.hpp"

namespace openjoey::lacooda64
{

    // -----------------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------------

    // Returns true when `o` is a well-formed operand of any supported kind.
    [[nodiscard]] constexpr bool validOperand(Operand o) noexcept
    {
        switch (tagOf(o))
        {
        case WordTag::None:
            return o == None;
        case WordTag::Address:
            return validAddress(o);
        case WordTag::Register:
            return registerBank(o) == RegisterBank::Value ||
                   registerBank(o) == RegisterBank::Address ||
                   registerBank(o) == RegisterBank::Flag;
        case WordTag::Immediate:
            return true;
        case WordTag::Control:
            return static_cast<Word>(controlReg(o)) <
                   static_cast<Word>(ControlReg::Count);
        default:
            return false;
        }
    }

    // Returns true when instruction `i` has a well-formed operation word and its
    // operands satisfy the per-opcode arity and writability rules.
    [[nodiscard]] constexpr bool validInstruction(const Instruction &i) noexcept
    {
        if (!isTag(operation(i), WordTag::Operation))
            return false;
        if (!validOperand(dst(i)) || !validOperand(src0(i)) || !validOperand(src1(i)))
            return false;

        switch (opcodeOf(operation(i)))
        {
        case Opcode::Nop:
        case Opcode::Halt:
            // No operands expected.
            return isNone(dst(i)) && isNone(src0(i)) && isNone(src1(i));

        case Opcode::Set:
        case Opcode::Copy:
            // dst must be writable; src0 must be present.
            return isWritable(dst(i)) && !isNone(src0(i));

        case Opcode::Load:
            // dst is a register; src0 must be present.
            return isRegister(dst(i)) && !isNone(src0(i));

        case Opcode::Store:
            // dst is a control/address-attribute; src0 must be present.
            return (isControl(dst(i)) ||
                    (isAddress(dst(i)) && isAttribute(dst(i)))) &&
                   !isNone(src0(i));

        case Opcode::Swap:
            // Both operand slots must be present.
            return !isNone(dst(i)) && !isNone(src0(i));

        case Opcode::Select:
            return isAddressRegister(dst(i)) && isAddress(src0(i));

        case Opcode::Count:
            return isValueRegister(dst(i)) && isAddress(src0(i));

        case Opcode::Move:
            // Both must be whole objects (not attributes).
            return isAddress(dst(i)) && isAddress(src0(i)) &&
                   isObject(dst(i)) && isObject(src0(i));

        case Opcode::Summon:
            // Destination is a slot/card object; src0 is the card being summoned.
            return isAddress(dst(i)) &&
                   isObject(dst(i)) &&
                   (levelOf(dst(i)) == AddressLevel::Slot ||
                    levelOf(dst(i)) == AddressLevel::Card) &&
                   !isNone(src0(i));

        case Opcode::Position:
        case Opcode::Negate:
            return isAddressSource(dst(i));

        case Opcode::Equip:
        case Opcode::Control:
            return isAddressSource(dst(i)) && isAddressSource(src0(i));

        case Opcode::Counter:
            return isAddressSource(dst(i)) && !isNone(src0(i));

        case Opcode::Restrict:
            // dst must name an attribute, not a whole object.
            return isAddress(dst(i)) && isAttribute(dst(i)) && !isNone(src0(i));

        case Opcode::Alu:
        {
            if (!isValueRegister(dst(i)) || !isValueSource(src0(i)))
                return false;

            switch (static_cast<AluOp>(subcodeOf(operation(i))))
            {
            case AluOp::Add:
            case AluOp::Subtract:
            case AluOp::Multiply:
            case AluOp::Divide:
            case AluOp::Modulo:
            case AluOp::Minimum:
            case AluOp::Maximum:
                return isValueSource(src1(i));

            case AluOp::Negate:
            case AluOp::Absolute:
                return isNone(src1(i));
            }
            return false;
        }

        case Opcode::Compare:
            // dst is a flag register; both sources must be present.
            return isFlagRegister(dst(i)) &&
                   !isNone(src0(i)) && !isNone(src1(i));

        case Opcode::Jump:
            // Unconditional: target is an immediate, no dst/src1.
            return isNone(dst(i)) &&
                   isImmediate(src0(i)) &&
                   isNone(src1(i));

        case Opcode::JumpIf:
            // src0 is a flag, src1 is the immediate target.
            return isNone(dst(i)) &&
                   isFlagRegister(src0(i)) &&
                   isImmediate(src1(i));

        case Opcode::Damage:
        case Opcode::GainLP:
        case Opcode::PayLP:
            return isAddressSource(dst(i)) && isValueSource(src0(i));

        case Opcode::Random:
            if (isValueRegister(dst(i)))
                return isImmediate(src0(i));
            if (isAddressRegister(dst(i)))
                return isAddress(src0(i));
            return false;

        case Opcode::Event:
            // An event must specify a non-None EventKind in its subcode.
            return subcodeOf(operation(i)) != sub(EventKind::None);

        case Opcode::Chain:
            // Chain subcodes are bounded by ChainOp::End.
            return subcodeOf(operation(i)) <= sub(ChainOp::End);
        }
        return false;
    }

    // A validated program is a flat sequence of instructions.
    using Trace = std::vector<Instruction>;

    // Trace-level validation errors reported by validateTrace().
    enum class ValidationError : Word
    {
        None = 0,
        InvalidInstruction, // An instruction failed validInstruction().
        JumpOutOfRange,     // A Jump/JumpIf target lies outside the trace.
    };

    // Outcome of validateTrace(): an error code plus the offending index.
    struct ValidationResult
    {
        ValidationError error{ValidationError::None};
        Word instruction{0}; // Index (PC) of the offending instruction.

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return error == ValidationError::None;
        }
        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return ok();
        }
    };

    // Validates a whole trace: every instruction must be well-formed and every
    // Jump/JumpIf target must be a valid instruction index.
    [[nodiscard]] inline ValidationResult validateTrace(const Trace &trace) noexcept
    {
        for (Word pc = 0; pc < trace.size(); ++pc)
        {
            const auto &i = trace[static_cast<std::size_t>(pc)];
            if (!validInstruction(i))
                return {ValidationError::InvalidInstruction, pc};

            const auto op = opcodeOf(operation(i));
            if (op == Opcode::Jump)
            {
                const SignedWord t = immediateValue(src0(i));
                if (t < 0 || static_cast<Word>(t) >= trace.size())
                    return {ValidationError::JumpOutOfRange, pc};
            }
            if (op == Opcode::JumpIf)
            {
                const SignedWord t = immediateValue(src1(i));
                if (t < 0 || static_cast<Word>(t) >= trace.size())
                    return {ValidationError::JumpOutOfRange, pc};
            }
        }
        return {};
    }

} // namespace openjoey::lacooda64
