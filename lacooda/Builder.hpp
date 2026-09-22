#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// High-level instruction builders (one per opcode family).

#include "Word.hpp"
#include "Address.hpp"
#include "Register.hpp"
#include "Immediate.hpp"
#include "Control.hpp"
#include "Opcode.hpp"
#include "Operation.hpp"
#include "Instruction.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Builders
// -----------------------------------------------------------------------------

// Nop: a no-op instruction.
[[nodiscard]] constexpr Instruction Nop() noexcept {
    return makeInstruction(Op(Opcode::Nop));
}

// Set: dst = src0 (typed move into a writable destination).
[[nodiscard]] constexpr Instruction Set(Operand d, Operand s) noexcept {
    return makeInstruction(Op(Opcode::Set), d, s);
}

// Load: dst register = value at address src0.
[[nodiscard]] constexpr Instruction Load(Operand registerDst, Operand source) noexcept {
    return makeInstruction(Op(Opcode::Load), registerDst, source);
}

// Store: write src0 into control/address-attribute dst.
[[nodiscard]] constexpr Instruction Store(Operand stateDst, Operand source) noexcept {
    return makeInstruction(Op(Opcode::Store), stateDst, source);
}

// Copy: dst = src0 (distinct encoding from Set).
[[nodiscard]] constexpr Instruction Copy(Operand d, Operand s) noexcept {
    return makeInstruction(Op(Opcode::Copy), d, s);
}

// Swap: exchange the values of dst and src0.
[[nodiscard]] constexpr Instruction Swap(Operand a, Operand b) noexcept {
    return makeInstruction(Op(Opcode::Swap), a, b);
}

// Select: pick `selected` into address register dst.
[[nodiscard]] constexpr Instruction Select(Operand addressReg,
                                           Address selected) noexcept {
    return makeInstruction(Op(Opcode::Select), addressReg, selected);
}

// Count: write the number of cards matching `container` into value register dst.
[[nodiscard]] constexpr Instruction Count(Operand valueReg,
                                        Address container) noexcept {
    return makeInstruction(Op(Opcode::Count), valueReg, container);
}

// Move: relocate cards from `source` into `destination` (src1 carries count).
[[nodiscard]] constexpr Instruction Move(
    Address destination,
    Address source,
    MoveMethod method,
    CauseKind cause = CauseKind::Unspecified,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Move, sub(method), flags, cause),
        destination,
        source
    );
}

// Summon: summon `card` into the slot/card `destination`.
[[nodiscard]] constexpr Instruction Summon(
    Address destination,
    Operand card,
    SummonMethod method,
    SummonMode mode = SummonMode::Default,
    CauseKind cause = CauseKind::SummonProcedure,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Summon, summonSubcode(method, mode), flags, cause),
        destination,
        card
    );
}

// Position: change the position of `target` (subcode = PositionOp).
[[nodiscard]] constexpr Instruction Position(
    Operand target,
    PositionOp method,
    CauseKind cause = CauseKind::Unspecified,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Position, sub(method), flags, cause),
        target
    );
}

// Equip: attach/detach/transfer equipment on `target` (subcode = EquipOp).
[[nodiscard]] constexpr Instruction Equip(
    Operand target,
    Operand equipment,
    EquipOp method = EquipOp::Attach,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Equip, sub(method), flags, cause),
        target,
        equipment
    );
}

// Counter: add/remove/transfer counters on `target`.
[[nodiscard]] constexpr Instruction Counter(
    Operand target,
    Operand amount,
    CounterOp method,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Counter, sub(method), flags, cause),
        target,
        amount
    );
}

// ChangeControl: take/give/swap/return control of `card` (subcode = ControlOp).
[[nodiscard]] constexpr Instruction ChangeControl(
    Operand destination,
    Operand card,
    ControlOp method,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Control, sub(method), flags, cause),
        destination,
        card
    );
}

// Negate: negate an activation/effect/summon/attack (subcode = NegateOp).
[[nodiscard]] constexpr Instruction Negate(
    Operand target,
    NegateOp method,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Negate, sub(method), flags, cause),
        target
    );
}

// Restrict: apply/clear/increment/decrement a play restriction on `attribute`.
[[nodiscard]] constexpr Instruction Restrict(
    Address attribute,
    Operand value,
    RestrictOp method = RestrictOp::Apply,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(
        Op(Opcode::Restrict, sub(method), flags, cause),
        attribute,
        value
    );
}

// Alu: binary integer op (dst = src0 <op> src1); subcode = AluOp.
[[nodiscard]] constexpr Instruction Alu(
    Operand valueReg,
    Operand lhs,
    Operand rhs,
    AluOp method
) noexcept {
    return makeInstruction(Op(Opcode::Alu, sub(method)), valueReg, lhs, rhs);
}

// AluUnary: unary integer op (dst = <op>(src0)); src1 is None.
[[nodiscard]] constexpr Instruction AluUnary(
    Operand valueReg,
    Operand value,
    AluOp method
) noexcept {
    return makeInstruction(Op(Opcode::Alu, sub(method)), valueReg, value, None);
}

// Compare: set flag register dst from comparing src0 vs src1 (subcode = CompareOp).
[[nodiscard]] constexpr Instruction Compare(
    Operand flagReg,
    Operand lhs,
    Operand rhs,
    CompareOp method = CompareOp::Equal
) noexcept {
    return makeInstruction(Op(Opcode::Compare, sub(method)), flagReg, lhs, rhs);
}

// Jump: unconditional jump to absolute PC `target`.
[[nodiscard]] constexpr Instruction Jump(ProgramCounter target) noexcept {
    return makeInstruction(
        Op(Opcode::Jump, sub(JumpCondition::Always)),
        None,
        Imm(static_cast<SignedWord>(target))
    );
}

// JumpIf: conditional jump to PC `target` when flag register `flagReg` holds.
[[nodiscard]] constexpr Instruction JumpIf(
    Operand flagReg,
    ProgramCounter target,
    JumpCondition condition = JumpCondition::True
) noexcept {
    return makeInstruction(
        Op(Opcode::JumpIf, sub(condition)),
        None,
        flagReg,
        Imm(static_cast<SignedWord>(target))
    );
}

// Damage: apply `amount` damage to `playerOrLP`.
[[nodiscard]] constexpr Instruction Damage(
    Operand playerOrLP,
    Operand amount,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(Op(Opcode::Damage, 0, flags, cause),
                           playerOrLP, amount);
}

// GainLP: increase LP of `playerOrLP` by `amount`.
[[nodiscard]] constexpr Instruction GainLP(
    Operand playerOrLP,
    Operand amount,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_None
) noexcept {
    return makeInstruction(Op(Opcode::GainLP, 0, flags, cause),
                           playerOrLP, amount);
}

// PayLP: decrease LP of `playerOrLP` by `amount` (a cost flag is set).
[[nodiscard]] constexpr Instruction PayLP(
    Operand playerOrLP,
    Operand amount,
    CauseKind cause = CauseKind::CardEffect,
    Word flags = Flag_Cost
) noexcept {
    return makeInstruction(Op(Opcode::PayLP, 0, flags, cause),
                           playerOrLP, amount);
}

// RecordedRandom: replay form — dst already holds the resolved result; src1
// optionally holds the domain size/context.
[[nodiscard]] constexpr Instruction RecordedRandom(
    Operand destinationRegister,
    RandomKind kind,
    Operand resolvedResult,
    Operand domain = None
) noexcept {
    return makeInstruction(Op(Opcode::Random, sub(kind)),
                           destinationRegister, resolvedResult, domain);
}

// Event: emit an engine event (subcode = EventKind) with subject/object/context.
[[nodiscard]] constexpr Instruction Event(
    EventKind kind,
    Operand subject = None,
    Operand object = None,
    Operand context = None,
    Word flags = Flag_None,
    CauseKind cause = CauseKind::Unspecified,
    Word aux = 0
) noexcept {
    return makeInstruction(
        Op(Opcode::Event, sub(kind), flags, cause, aux),
        subject, object, context
    );
}

// Chain: drive chain-resolution protocol (subcode = ChainOp).
[[nodiscard]] constexpr Instruction Chain(
    ChainOp method,
    Operand subject = None,
    Operand context = None
) noexcept {
    return makeInstruction(Op(Opcode::Chain, sub(method)),
                           subject, context);
}

// Halt: stop instruction execution.
[[nodiscard]] constexpr Instruction Halt() noexcept {
    return makeInstruction(Op(Opcode::Halt));
}

} // namespace openjoey::lacooda64
