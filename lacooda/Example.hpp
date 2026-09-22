#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Worked examples matching the compact assembly language.

#include "Word.hpp"
#include "Address.hpp"
#include "Register.hpp"
#include "Immediate.hpp"
#include "Opcode.hpp"
#include "Operation.hpp"
#include "Instruction.hpp"
#include "Builder.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Examples matching the compact assembly language
// -----------------------------------------------------------------------------

namespace example {

// Example addresses/attributes use intentionally arbitrary ruleset IDs.
inline constexpr AttributeId ATTR_ATK = 1;
inline constexpr AttributeId ATTR_LP  = 2;
inline constexpr ZoneId ZONE_MONSTER  = 1;
inline constexpr ZoneId ZONE_DECK     = 2;
inline constexpr ZoneId ZONE_HAND     = 3;

// Handy concrete addresses used by the worked example below.
inline constexpr Address P0_LP =
    Player(0, ATTR_LP);

inline constexpr Address P0_DECK =
    Zone(0, ZONE_DECK, Self);

inline constexpr Address P0_HAND =
    Zone(0, ZONE_HAND, Self);

inline constexpr Address P0_MONSTERS =
    Zone(0, ZONE_MONSTER, Self);

// Assembly:
//
//   COUNT    V0, P0.FIELD.MONSTER
//   COMPARE  F0, V0, #3
//   JUMPIF   F0, effect
//   HALT
//
// effect:
//   MOVE     DRAW, P0.DECK, P0.HAND, #2
//   HALT
//
// `MOVE DRAW ... #2` needs a count operand. The fixed 4-word machine expresses
// that count through src1, while source/destination occupy dst/src0.
inline constexpr Instruction CountMonsters =
    Count(V(0), P0_MONSTERS);

inline constexpr Instruction CompareThree =
    Compare(F(0), V(0), Imm(3), CompareOp::GreaterEqual);

inline constexpr Instruction JumpToEffect =
    JumpIf(F(0), 4);

inline constexpr Instruction Stop =
    Halt();

inline constexpr Instruction DrawTwo =
    makeInstruction(
        Op(Opcode::Move, sub(MoveMethod::Draw), Flag_None, CauseKind::CardEffect),
        P0_HAND,
        P0_DECK,
        Imm(2)
    );

} // namespace example

} // namespace openjoey::lacooda64
