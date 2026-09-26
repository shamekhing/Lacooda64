#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// High-level instruction builders (one per opcode family).

#include "Address.hpp"
#include "Control.hpp"
#include "Immediate.hpp"
#include "Instruction.hpp"
#include "Opcode.hpp"
#include "Operation.hpp"
#include "Register.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Builders
// -----------------------------------------------------------------------------

// Nop: a no-op instruction.
[[nodiscard]] constexpr Instruction Nop() noexcept { return MakeInstruction(Op(Opcode::kNop)); }

// Set: dst = src0 (typed move into a writable destination).
[[nodiscard]] constexpr Instruction Set(Operand d, Operand s) noexcept { return MakeInstruction(Op(Opcode::kSet), d, s); }

// Load: dst register = value at address src0.
[[nodiscard]] constexpr Instruction Load(Operand dst_reg, Operand source) noexcept { return MakeInstruction(Op(Opcode::kLoad), dst_reg, source); }

// Store: write src0 into control/address-attribute dst.
[[nodiscard]] constexpr Instruction Store(Operand state_dst, Operand source) noexcept { return MakeInstruction(Op(Opcode::kStore), state_dst, source); }

// Copy: dst = src0 (distinct encoding from Set).
[[nodiscard]] constexpr Instruction Copy(Operand d, Operand s) noexcept { return MakeInstruction(Op(Opcode::kCopy), d, s); }

// Swap: exchange the values of dst and src0.
[[nodiscard]] constexpr Instruction Swap(Operand a, Operand b) noexcept { return MakeInstruction(Op(Opcode::kSwap), a, b); }

// Select: pick `selected` into address register dst.
[[nodiscard]] constexpr Instruction Select(Operand address_reg, Address selected) noexcept { return MakeInstruction(Op(Opcode::kSelect), address_reg, selected); }

// Count: write the number of cards matching `container` into value reg dst.
[[nodiscard]] constexpr Instruction Count(Operand value_reg, Address container) noexcept { return MakeInstruction(Op(Opcode::kCount), value_reg, container); }

// Move: relocate cards from `source` into `destination`; src1 is kNone.
// For an explicit count in src1, use MakeInstruction (see example::kDrawTwo).
[[nodiscard]] constexpr Instruction Move(Address destination, Address source, MoveMethod method, CauseKind cause = CauseKind::kUnspecified, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kMove, Sub(method), flags, cause), destination, source); }

// Summon: summon `card` into the slot/card `destination`.
[[nodiscard]] constexpr Instruction Summon(Address destination, Operand card, SummonMethod method, SummonMode mode = SummonMode::kDefault, CauseKind cause = CauseKind::kSummonProcedure, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kSummon, SummonSubcode(method, mode), flags, cause), destination, card); }

// Position: change the position of `target` (subcode = PositionOp).
[[nodiscard]] constexpr Instruction Position(Operand target, PositionOp method, CauseKind cause = CauseKind::kUnspecified, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kPosition, Sub(method), flags, cause), target); }

// Equip: attach/detach/transfer equipment on `target` (subcode = EquipOp).
[[nodiscard]] constexpr Instruction Equip(Operand target, Operand equipment, EquipOp method = EquipOp::kAttach, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kEquip, Sub(method), flags, cause), target, equipment); }

// Counter: add/remove/transfer counters on `target`.
[[nodiscard]] constexpr Instruction Counter(Operand target, Operand amount, CounterOp method, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kCounter, Sub(method), flags, cause), target, amount); }

// Transfer counters between explicit attributes. Omit amount to transfer all.
[[nodiscard]] constexpr Instruction TransferCounters(Operand destination, Operand source, Operand amount = kNone) noexcept { return MakeInstruction(Op(Opcode::kCounter, Sub(CounterOp::kTransfer)), destination, source, amount); }

// ChangeControl: take/give/swap/return control of `card` (subcode = ControlOp).
[[nodiscard]] constexpr Instruction ChangeControl(Operand destination, Operand card, ControlOp method, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kControl, Sub(method), flags, cause), destination, card); }

// Negate: negate an activation/effect/summon/attack (subcode = NegateOp).
[[nodiscard]] constexpr Instruction Negate(Operand target, NegateOp method, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kNegate, Sub(method), flags, cause), target); }

// Restrict: apply/clear/increment/decrement a play restriction on `attribute`.
[[nodiscard]] constexpr Instruction Restrict(Address attribute, Operand value, RestrictOp method = RestrictOp::kApply, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kRestrict, Sub(method), flags, cause), attribute, value); }

// Alu: binary integer op (dst = src0 <op> src1); subcode = AluOp.
[[nodiscard]] constexpr Instruction Alu(Operand value_reg, Operand lhs, Operand rhs, AluOp method) noexcept { return MakeInstruction(Op(Opcode::kAlu, Sub(method)), value_reg, lhs, rhs); }

// AluUnary: unary integer op (dst = <op>(src0)); src1 is kNone.
[[nodiscard]] constexpr Instruction AluUnary(Operand value_reg, Operand value, AluOp method) noexcept { return MakeInstruction(Op(Opcode::kAlu, Sub(method)), value_reg, value, kNone); }

// Compare: set flag register dst from comparing src0 vs src1 (subcode =
// CompareOp).
[[nodiscard]] constexpr Instruction Compare(Operand flag_reg, Operand lhs, Operand rhs, CompareOp method = CompareOp::kEqual) noexcept { return MakeInstruction(Op(Opcode::kCompare, Sub(method)), flag_reg, lhs, rhs); }

// Encode an unrepresentable PC as -1 so ValidateTrace rejects it rather than
// accepting a wrapped target. Normal immediate packing still masks its input.
[[nodiscard]] constexpr Operand JumpTarget(ProgramCounter target) noexcept { return Imm(target <= static_cast<Word>(kImmediateMax) ? static_cast<SignedWord>(target) : SignedWord{-1}); }

// Jump: unconditional jump to absolute PC `target`.
[[nodiscard]] constexpr Instruction Jump(ProgramCounter target) noexcept { return MakeInstruction(Op(Opcode::kJump, Sub(JumpCondition::kAlways)), kNone, JumpTarget(target)); }

// JumpIf: conditional jump to PC `target` when flag register `flag_reg` holds.
[[nodiscard]] constexpr Instruction JumpIf(Operand flag_reg, ProgramCounter target, JumpCondition condition = JumpCondition::kTrue) noexcept { return MakeInstruction(Op(Opcode::kJumpIf, Sub(condition)), kNone, flag_reg, JumpTarget(target)); }

// Damage: apply `amount` damage to `player_or_lp`.
[[nodiscard]] constexpr Instruction Damage(Operand player_or_lp, Operand amount, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kDamage, 0, flags, cause), player_or_lp, amount); }

// GainLp: increase LP of `player_or_lp` by `amount`.
[[nodiscard]] constexpr Instruction GainLp(Operand player_or_lp, Operand amount, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagNone) noexcept { return MakeInstruction(Op(Opcode::kGainLp, 0, flags, cause), player_or_lp, amount); }

// PayLp: decrease LP of `player_or_lp` by `amount` (a cost flag is set).
[[nodiscard]] constexpr Instruction PayLp(Operand player_or_lp, Operand amount, CauseKind cause = CauseKind::kCardEffect, Word flags = kFlagCost) noexcept { return MakeInstruction(Op(Opcode::kPayLp, 0, flags, cause), player_or_lp, amount); }

// RecordedRandom: replay form — src0 carries the resolved result to write
// into dst; src1 optionally holds the domain size/context.
[[nodiscard]] constexpr Instruction RecordedRandom(Operand destination_reg, RandomKind kind, Operand resolved_result, Operand domain = kNone) noexcept { return MakeInstruction(Op(Opcode::kRandom, Sub(kind)), destination_reg, resolved_result, domain); }

// Event: emit an engine event (subcode = EventKind) with
// subject/object/context.
[[nodiscard]] constexpr Instruction Event(EventKind kind, Operand subject = kNone, Operand object = kNone, Operand context = kNone, Word flags = kFlagNone, CauseKind cause = CauseKind::kUnspecified, Word aux = 0) noexcept { return MakeInstruction(Op(Opcode::kEvent, Sub(kind), flags, cause, aux), subject, object, context); }

// Chain: drive chain-resolution protocol (subcode = ChainOp).
[[nodiscard]] constexpr Instruction Chain(ChainOp method, Operand subject = kNone, Operand context = kNone) noexcept { return MakeInstruction(Op(Opcode::kChain, Sub(method)), subject, context); }

// General program primitives: no card names or rule predicates are encoded.
[[nodiscard]] constexpr Instruction Enumerate(Operand dst, Operand container) noexcept { return MakeInstruction(Op(Opcode::kEnumerate), dst, container); }
[[nodiscard]] constexpr Instruction At(Operand dst, Operand collection, Operand index) noexcept { return MakeInstruction(Op(Opcode::kAt), dst, collection, index); }
[[nodiscard]] constexpr Instruction Append(Operand collection, Operand object) noexcept { return MakeInstruction(Op(Opcode::kAppend), collection, object); }
[[nodiscard]] constexpr Instruction Length(Operand dst, Operand collection) noexcept { return MakeInstruction(Op(Opcode::kLength), dst, collection); }
[[nodiscard]] constexpr Instruction Attribute(Operand dst, Operand object, Operand field) noexcept { return MakeInstruction(Op(Opcode::kAttribute), dst, object, field); }
[[nodiscard]] constexpr Instruction Choose(Operand dst, Operand collection, Operand player) noexcept { return MakeInstruction(Op(Opcode::kChoose), dst, collection, player); }
[[nodiscard]] constexpr Instruction Random(Operand dst, Operand bound, RandomKind kind = RandomKind::kDie) noexcept { return MakeInstruction(Op(Opcode::kRandom, Sub(kind)), dst, kNone, bound); }
[[nodiscard]] constexpr Instruction Schedule(Operand dst, ProgramCounter pc, Operand time) noexcept { return MakeInstruction(Op(Opcode::kSchedule), dst, JumpTarget(pc), time); }
[[nodiscard]] constexpr Instruction Subscribe(Operand dst, ProgramCounter pc, Operand event) noexcept { return MakeInstruction(Op(Opcode::kSubscribe), dst, JumpTarget(pc), event); }
[[nodiscard]] constexpr Instruction Cancel(Operand registration) noexcept { return MakeInstruction(Op(Opcode::kCancel), kNone, registration); }

[[nodiscard]] constexpr Instruction Modify(Operand handle, Operand attribute, Operand value, ModifierOp method) noexcept { return MakeInstruction(Op(Opcode::kModify, Sub(method)), handle, attribute, value); }
[[nodiscard]] constexpr Instruction Unmodify(Operand handle) noexcept { return MakeInstruction(Op(Opcode::kUnmodify), kNone, handle); }

[[nodiscard]] constexpr Instruction History(Operand dst, HistoryField field, Operand index = kNone) noexcept { return MakeInstruction(Op(Opcode::kHistory, Sub(field)), dst, index); }
[[nodiscard]] constexpr Instruction Stage(Operand dst, ProgramCounter pc, Operand event) noexcept { return MakeInstruction(Op(Opcode::kStage), dst, JumpTarget(pc), event); }
[[nodiscard]] constexpr Instruction Commit(Operand handle) noexcept { return MakeInstruction(Op(Opcode::kCommit), kNone, handle); }

// Halt: stop instruction execution.
[[nodiscard]] constexpr Instruction Halt() noexcept { return MakeInstruction(Op(Opcode::kHalt)); }

}  // namespace openjoey::lacooda64
