#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Opcodes, subcode enums and instruction flags — all 64-bit enum values.

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Opcodes and subcodes — all 64-bit enum values
// -----------------------------------------------------------------------------

// Primary opcode of an operation word. Each opcode names an opcode family; the
// subcode/flags/cause/aux fields disambiguate the variant. The numeric values
// must stay stable because they are the serialised wire format.
enum class Opcode : Word {
  kNop = 0,    // No operation (operand slots are kNone).
  kSet,        // dst = src0  (typed move into a writable target).
  kLoad,       // dst register = value of address src0.
  kStore,      // address/control dst = src0 (writes state, e.g. LP).
  kCopy,       // dst = src0 (same semantics as Set, distinct encoding).
  kSwap,       // Exchange the values of dst and src0.
  kSelect,     // Select the literal src0 address into address register dst.
  kCount,      // Count cards matching src0 into value register dst.
  kMove,       // Move/reposition cards from src0 into dst (subcode=method).
  kSummon,     // Summon a card (subcode packs method + mode).
  kPosition,   // Change card position (subcode = PositionOp).
  kEquip,      // Attach/detach/transfer equipment (subcode = EquipOp).
  kCounter,    // Add/remove/transfer counters on a card.
  kControl,    // Take/give/swap/return control of a card (subcode=ControlOp).
  kNegate,     // Negate an activation/effect/summon/attack.
  kRestrict,   // Apply/clear/increment/decrement a play restriction.
  kAlu,        // Integer ALU op (subcode = AluOp).
  kCompare,    // Compare src0 vs src1 into a flag register (subcode=CompareOp).
  kJump,       // Unconditional jump to src0 (an immediate PC).
  kJumpIf,     // Conditional jump to src1 (immediate PC) if src0 holds.
  kDamage,     // Apply damage (dst target, src0 amount).
  kGainLp,     // Gain LP (dst target, src0 amount).
  kPayLp,      // Pay LP cost (dst target, src0 amount).
  kRandom,     // Produce a random value (subcode = RandomKind).
  kEvent,      // Emit an engine event (subcode = EventKind).
  kChain,      // Chain-flow control (subcode = ChainOp).
  kHalt,       // Stop execution (operand slots are kNone).
  kEnumerate,  // dst value register = snapshot of objects in src0 container.
  kAt,         // dst address register = src0 collection element at src1 index.
  kAppend,     // Append src0 address to collection in dst value register.
  kLength,     // dst value register = size of src0 collection.
  kAttribute,  // dst address register = src0 object with attribute src1.
  kChoose,     // Pause for selection from src0 collection; src1 = player.
  kSchedule,   // Capture frame at src0 PC, due at logical time src1; dst = id.
  kSubscribe,  // Capture frame at src0 PC, invoked for event kind src1; dst = id.
  kCancel,     // Remove registration identified by src0.
  kModify,     // dst = modifier handle; src0 = attribute; src1 = value.
  kHistory,    // Read history size or a field of a recorded event.
  kStage,      // Capture one instruction at src0 PC before applying it.
  kCommit,     // Apply or cancel the staged instruction identified by src0.
  kUnmodify,   // Remove modifier identified by src0.

};

enum class HistoryField : Word { kCount = 0, kKind, kSubject, kObject, kContext };

// Modifiers compose in registration order over the stored base value.
enum class ModifierOp : Word { kAdd = 0, kSet, kMultiply, kDivide };

// How cards move between locations (fits in a Move subcode).
enum class MoveMethod : Word {
  kRelocate = 0,    // Move within/across zones without a special rule.
  kDraw,            // Draw from a deck/hand source.
  kSearch,          // Search a deck/library for a card.
  kMill,            // Send deck cards to the graveyard.
  kDiscard,         // Send hand cards to the graveyard.
  kTribute,         // Tribute a card (e.g. for a summon).
  kDestroy,         // Destroy and send a card.
  kSend,            // Generic send to a specified zone.
  kBanish,          // Remove from play entirely.
  kReturn,          // Return a card from the GY/banish zone.
  kAttachMaterial,  // Attach as material (overlay/upgrades).
  kDetachMaterial,  // Detach previously attached material.
};

// Summon procedure kind (low 8 bits of the Summon subcode).
enum class SummonMethod : Word {
  kNormal = 0,  // Normal/tribute summon.
  kTribute,     // Tribute summon (costs tributes).
  kFlip,        // Flip summon.
  kSpecial,     // Special summon.
  kFusion,      // Fusion summon.
  kRitual,      // Ritual summon.
  kToken,       // Token summon.
};

// Initial battle position of a summoned card (high 4 bits of Summon subcode).
enum class SummonMode : Word {
  kDefault = 0,
  kFaceUpAttack,
  kFaceUpDefense,
  kFaceDownDefense,
  kSet,
};

// ALU operation (ALU subcode).
enum class AluOp : Word {
  kAdd = 0,
  kSubtract,
  kMultiply,
  kDivide,
  kModulo,
  kMinimum,
  kMaximum,
  kNegate,
  kAbsolute,
};

// Comparison operation (Compare subcode) producing a flag result.
enum class CompareOp : Word {
  kEqual = 0,
  kNotEqual,
  kLess,
  kLessEqual,
  kGreater,
  kGreaterEqual,
};

// Position change operation (Position subcode).
enum class PositionOp : Word {
  kAttack = 0,
  kDefense,
  kFaceUp,
  kFaceDown,
  kFaceUpAttack,
  kFaceUpDefense,
  kFaceDownDefense,
  kToggle,
};

// What kind of chainable effect is being negated (Negate subcode).
enum class NegateOp : Word {
  kActivation = 0,  // Negate an activation.
  kEffect,          // Negate an effect.
  kSummon,          // Negate a summon.
  kAttack,          // Negate an attack.
};

// Attach/detach/transfer for equip-style effects (Equip subcode).
enum class EquipOp : Word {
  kAttach = 0,
  kDetach,
  kTransfer,
};

// Counter manipulation operation (Counter subcode).
enum class CounterOp : Word {
  kPlace = 0,
  kRemove,
  kTransfer,
  kSet,
};

// Control-change method (Control subcode).
enum class ControlOp : Word {
  kTake = 0,
  kGive,
  kSwap,
  kReturn,
};

// Restriction lifecycle operation (Restrict subcode).
enum class RestrictOp : Word {
  kApply = 0,
  kClear,
  kIncrement,
  kDecrement,
};

// Kind of random event (Random subcode).
enum class RandomKind : Word {
  kDie = 0,      // Roll a die.
  kCoin,         // Toss a coin.
  kChoice,       // Arbitrary player choice.
  kRandomCard,   // Select a random card from a set.
  kShuffleSwap,  // Shuffle and swap ordering.
  kCutPoint,     // A deck cut point.
};

// Step of the chain-resolution protocol (Chain subcode).
enum class ChainOp : Word {
  kBegin = 0,
  kPush,          // Push a new chain link.
  kPass,          // Pass priority.
  kBeginResolve,  // Start resolving a chain.
  kResolveLink,   // Resolve the current link's effects.
  kPop,           // Pop a resolved chain link.
  kEndResolve,    // Finish resolving the current chain.
  kEnd,           // End the chain phase.
};

// Condition tested by JumpIf (Jump subcode).
enum class JumpCondition : Word {
  kAlways = 0,
  kZero,
  kNotZero,
  kTrue,
  kFalse,
  kEqual,
  kNotEqual,
  kLess,
  kLessEqual,
  kGreater,
  kGreaterEqual,
};

// Kinds of engine events (Event subcode).
enum class EventKind : Word {
  kNone = 0,
  kTurnBegin,
  kTurnEnd,
  kPhaseBegin,
  kPhaseEnd,
  kStepBegin,
  kStepEnd,
  kTrigger,   // A card trigger condition fired.
  kActivate,  // An effect was activated.
  kResolveBegin,
  kResolveEnd,
  kActivationNegated,  // An activation was negated.
  kEffectNegated,      // An effect was negated.
  kDeclare,            // A declaration (attack/target/choice).
  kSelect,             // A selection prompt was shown.
  kTarget,             // A target was chosen.
  kReveal,             // Cards were revealed.
  kInspect,            // Cards were inspected (e.g. opponent's hand).
  kExcavate,           // Cards were excavated from a deck.
  kDieRolled,
  kCoinTossed,
  kShuffle,
  kCut,
  kWouldMove,    // "Would" event: before a move resolves.
  kMoved,        // A move completed.
  kReplacement,  // A replacement effect was applied.
  kWouldSummon,
  kSummoned,
  kPositionChanged,
  kControlChanged,
  kEquipped,
  kUnequipped,
  kCounterChanged,
  kAttackDeclared,
  kAttackTargeted,
  kAttackDirect,
  kAttackCanceled,
  kBattleReplay,
  kBattle,
  kBattleDamage,
  kDeckOut,
  kWin,
  kDraw,
};

// Why an instruction happened (CauseKind, stored in the operation word).
enum class CauseKind : Word {
  kUnspecified = 0,
  kRule,             // Game-rule driven.
  kPlayerAction,     // Explicitly chosen by a player.
  kCardEffect,       // Produced by a card's effect.
  kBattle,           // Produced by the battle step.
  kSummonProcedure,  // Part of a summon procedure.
  kMaintenance,      // Maintenance cost/effect.
  kReplacement,      // From a replacement effect.
};

// Instruction flags (12-bit field; combinable bit flags).
enum InstructionFlag : Word {
  kFlagNone = 0,
  kFlagForced = Word{1} << 0,       // The instruction cannot be declined.
  kFlagOptional = Word{1} << 1,     // The instruction is player-optional.
  kFlagCost = Word{1} << 2,         // The instruction is a cost payment.
  kFlagReplacement = Word{1} << 3,  // Part of a replacement effect layer.
  kFlagPublic = Word{1} << 4,       // The effect/state is public knowledge.
  kFlagHidden = Word{1} << 5,       // The effect/state is private/hidden.
  kFlagMandatory = Word{1} << 6,    // The instruction must be performed.
};

}  // namespace openjoey::lacooda64
