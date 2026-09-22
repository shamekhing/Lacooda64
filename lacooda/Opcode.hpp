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
    Nop = 0,        // No operation (operand slots are None).
    Set,            // dst = src0  (typed move into a writable target).
    Load,           // dst register = value of address src0.
    Store,          // address/control dst = src0 (writes state, e.g. LP).
    Copy,           // dst = src0 (same semantics as Set, distinct encoding).
    Swap,           // Exchange the values of dst and src0.
    Select,         // Pick an address from an address register into a slot.
    Count,          // Count cards matching src0 into value register dst.
    Move,           // Move/reposition cards from src0 into dst (subcode = method).
    Summon,         // Summon a card (subcode packs method + mode).
    Position,       // Change card position (subcode = PositionOp).
    Equip,          // Attach/detach/transfer equipment (subcode = EquipOp).
    Counter,        // Add/remove/transfer counters on a card.
    Control,        // Take/give/swap/return control of a card (subcode = ControlOp).
    Negate,         // Negate an activation/effect/summon/attack.
    Restrict,       // Apply/clear/increment/decrement a play restriction.
    Alu,            // Integer ALU op (subcode = AluOp).
    Compare,        // Compare src0 vs src1 into a flag register (subcode = CompareOp).
    Jump,           // Unconditional jump to src0 (an immediate PC).
    JumpIf,         // Conditional jump to src1 (immediate PC) if src0 (flag) holds.
    Damage,         // Apply damage (dst target, src0 amount).
    GainLP,         // Gain LP (dst target, src0 amount).
    PayLP,         // Pay LP cost (dst target, src0 amount).
    Random,         // Produce a random value (subcode = RandomKind).
    Event,          // Emit an engine event (subcode = EventKind).
    Chain,          // Chain-flow control (subcode = ChainOp).
    Halt,           // Stop execution (operand slots are None).
};

// How cards move between locations (fits in a Move subcode).
enum class MoveMethod : Word {
    Relocate = 0,   // Move within/across zones without a special rule.
    Draw,           // Draw from a deck/hand source.
    Search,         // Search a deck/library for a card.
    Mill,           // Send deck cards to the graveyard.
    Discard,        // Send hand cards to the graveyard.
    Tribute,        // Tribute a card (e.g. for a summon).
    Destroy,        // Destroy and send a card.
    Send,           // Generic send to a specified zone.
    Banish,         // Remove from play entirely.
    Return,         // Return a card from the GY/banish zone.
    AttachMaterial, // Attach as material (overlay/upgrades).
    DetachMaterial, // Detach previously attached material.
};

// Summon procedure kind (low 8 bits of the Summon subcode).
enum class SummonMethod : Word {
    Normal = 0, // Normal/tribute summon.
    Tribute,    // Tribute summon (costs tributes).
    Flip,       // Flip summon.
    Special,    // Special summon.
    Fusion,     // Fusion summon.
    Ritual,     // Ritual summon.
    Token,      // Token summon.
};

// Initial battle position of a summoned card (high 4 bits of the Summon subcode).
enum class SummonMode : Word {
    Default = 0,
    FaceUpAttack,
    FaceUpDefense,
    FaceDownDefense,
    Set,
};

// ALU operation (ALU subcode).
enum class AluOp : Word {
    Add = 0,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Minimum,
    Maximum,
    Negate,
    Absolute,
};

// Comparison operation (Compare subcode) producing a flag result.
enum class CompareOp : Word {
    Equal = 0,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

// Position change operation (Position subcode).
enum class PositionOp : Word {
    Attack = 0,
    Defense,
    FaceUp,
    FaceDown,
    FaceUpAttack,
    FaceUpDefense,
    FaceDownDefense,
    Toggle,
};

// What kind of chainable effect is being negated (Negate subcode).
enum class NegateOp : Word {
    Activation = 0, // Negate an activation.
    Effect,         // Negate an effect.
    Summon,         // Negate a summon.
    Attack,         // Negate an attack.
};

// Attach/detach/transfer for equip-style effects (Equip subcode).
enum class EquipOp : Word {
    Attach = 0,
    Detach,
    Transfer,
};

// Counter manipulation operation (Counter subcode).
enum class CounterOp : Word {
    Place = 0,
    Remove,
    Transfer,
    Set,
};

// Control-change method (Control subcode).
enum class ControlOp : Word {
    Take = 0,
    Give,
    Swap,
    Return,
};

// Restriction lifecycle operation (Restrict subcode).
enum class RestrictOp : Word {
    Apply = 0,
    Clear,
    Increment,
    Decrement,
};

// Kind of random event (Random subcode).
enum class RandomKind : Word {
    Die = 0,        // Roll a die.
    Coin,           // Toss a coin.
    Choice,         // Arbitrary player choice.
    RandomCard,     // Select a random card from a set.
    ShuffleSwap,    // Shuffle and swap ordering.
    CutPoint,       // A deck cut point.
};

// Step of the chain-resolution protocol (Chain subcode).
enum class ChainOp : Word {
    Begin = 0,
    Push,            // Push a new chain link.
    Pass,            // Pass priority.
    BeginResolve,    // Start resolving a chain.
    ResolveLink,     // Resolve the current link's effects.
    Pop,             // Pop a resolved chain link.
    EndResolve,      // Finish resolving the current chain.
    End,             // End the chain phase.
};

// Condition tested by JumpIf (Jump subcode).
enum class JumpCondition : Word {
    Always = 0,
    Zero,
    NotZero,
    True,
    False,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

// Kinds of engine events (Event subcode).
enum class EventKind : Word {
    None = 0,
    TurnBegin,
    TurnEnd,
    PhaseBegin,
    PhaseEnd,
    StepBegin,
    StepEnd,
    Trigger,           // A card trigger condition fired.
    Activate,          // An effect was activated.
    ResolveBegin,
    ResolveEnd,
    ActivationNegated, // An activation was negated.
    EffectNegated,     // An effect was negated.
    Declare,           // A declaration (attack/target/choice).
    Select,            // A selection prompt was shown.
    Target,            // A target was chosen.
    Reveal,            // Cards were revealed.
    Inspect,           // Cards were inspected (e.g. opponent's hand).
    Excavate,          // Cards were excavated from a deck.
    DieRolled,
    CoinTossed,
    Shuffle,
    Cut,
    WouldMove,         // "Would" event: before a move resolves.
    Moved,             // A move completed.
    Replacement,       // A replacement effect was applied.
    WouldSummon,
    Summoned,
    PositionChanged,
    ControlChanged,
    Equipped,
    Unequipped,
    CounterChanged,
    AttackDeclared,
    AttackTargeted,
    AttackDirect,
    AttackCanceled,
    BattleReplay,
    Battle,
    BattleDamage,
    DeckOut,
    Win,
    Draw,
};

// Why an instruction happened (CauseKind, stored in the operation word).
enum class CauseKind : Word {
    Unspecified = 0,
    Rule,              // Game-rule driven.
    PlayerAction,      // Explicitly chosen by a player.
    CardEffect,        // Produced by a card's effect.
    Battle,            // Produced by the battle step.
    SummonProcedure,   // Part of a summon procedure.
    Maintenance,       // Maintenance cost/effect.
    Replacement,       // From a replacement effect.
};

// Instruction flags (12-bit field; combinable bit flags).
enum InstructionFlag : Word {
    Flag_None        = 0,
    Flag_Forced     = Word{1} << 0,  // The instruction cannot be declined.
    Flag_Optional   = Word{1} << 1,  // The instruction is player-optional.
    Flag_Cost       = Word{1} << 2,  // The instruction is a cost payment.
    Flag_Replacement = Word{1} << 3, // Part of a replacement effect layer.
    Flag_Public     = Word{1} << 4,  // The effect/state is public knowledge.
    Flag_Hidden     = Word{1} << 5,  // The effect/state is private/hidden.
    Flag_Mandatory  = Word{1} << 6,  // The instruction must be performed.
};

} // namespace openjoey::lacooda64
