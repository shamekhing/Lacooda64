#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Worked examples matching the compact assembly language.

#include "Address.hpp"
#include "Builder.hpp"
#include "Immediate.hpp"
#include "Instruction.hpp"
#include "Opcode.hpp"
#include "Operation.hpp"
#include "Register.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Examples matching the compact assembly language
// -----------------------------------------------------------------------------

namespace example {

// Example addresses/attributes use intentionally arbitrary ruleset IDs.
inline constexpr AttributeId kAttrAtk = 1;
inline constexpr AttributeId kAttrLp = 2;
inline constexpr ZoneId kZoneMonster = 1;
inline constexpr ZoneId kZoneDeck = 2;
inline constexpr ZoneId kZoneHand = 3;

// Handy concrete addresses used by the worked example below.
inline constexpr Address kP0Lp = Player(0, kAttrLp);

inline constexpr Address kP0Deck = Zone(0, kZoneDeck, kSelf);

inline constexpr Address kP0Hand = Zone(0, kZoneHand, kSelf);

inline constexpr Address kP0Monsters = Zone(0, kZoneMonster, kSelf);

// Assembly:
//
//   COUNT    V0, P0.FIELD.MONSTER
//   COMPARE  F0, V0, #3
//   JUMPIF   F0, effect
//   HALT
//
// effect:
//   MOVE     DRAW, P0.HAND, P0.DECK, #2
//   HALT
//
// `MOVE DRAW ... #2` needs a count operand. The fixed 4-word machine expresses
// that count through src1, while destination/source occupy dst/src0.
inline constexpr Instruction kCountMonsters = Count(V(0), kP0Monsters);

inline constexpr Instruction kCompareThree =
    Compare(F(0), V(0), Imm(3), CompareOp::kGreaterEqual);

inline constexpr Instruction kJumpToEffect = JumpIf(F(0), 4);

inline constexpr Instruction kStop = Halt();

inline constexpr Instruction kDrawTwo =
    MakeInstruction(Op(Opcode::kMove, Sub(MoveMethod::kDraw), kFlagNone,
                       CauseKind::kCardEffect),
                    kP0Hand, kP0Deck, Imm(2));

}  // namespace example

}  // namespace openjoey::lacooda64
