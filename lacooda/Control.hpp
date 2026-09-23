#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Control-register word and the control-state array.

#include <array>
#include <cstddef>

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Control-register word
//
// Payload bits (60-bit selector; current valid values are 0..5):
//
//   payload bit
//   59..0
//   +------------------------------------------------------------+
//   | ControlReg selector                                        |
//   +------------------------------------------------------------+
//                               60
//
// Full 64-bit word:
//   63..60 = WordTag::kControl
//
// 0 Turn, 1 Phase, 2 Step, 3 Chain, 4 Effect, 5 ProgramCounter.
// The payload selects a ControlState cell; it does not hold its value.
// kCount is a size sentinel, not a usable selector.
// -----------------------------------------------------------------------------

// Control registers index the engine's global state. The numeric value doubles
// as the array index into a ControlState and as the payload of a Control
// operand.
enum class ControlReg : Word {
  kTurn = 0,        // Current turn number.
  kPhase,           // Current phase (e.g. Draw, Standby, Main).
  kStep,            // Current sub-step within a phase.
  kChain,           // Current chain layer being resolved.
  kEffect,          // Current effect / frame identifier.
  kProgramCounter,  // Index into the trace of next instruction.
  kCount,           // Sentinel: number of control registers.
};

// Builds a Control-tagged operand selecting one of the control registers.
[[nodiscard]] constexpr Operand Control(ControlReg r) noexcept {
  return MakeTagged(WordTag::kControl, static_cast<Word>(r));
}

// Decodes which control register a Control-tagged operand selects.
[[nodiscard]] constexpr ControlReg ControlRegOf(Operand w) noexcept {
  return static_cast<ControlReg>(PayloadOf(w));
}

// True when `w` is a Control operand.
[[nodiscard]] constexpr bool IsControl(Operand w) noexcept {
  return IsTag(w, WordTag::kControl);
}

// All control values are Words; ControlState is the fixed register file sized
// to exactly ControlReg::kCount entries.
using ControlState =
    std::array<Word, static_cast<std::size_t>(ControlReg::kCount)>;

// Maps a ControlReg to its index within a ControlState array.
[[nodiscard]] constexpr std::size_t ControlIndex(ControlReg r) noexcept {
  return static_cast<std::size_t>(r);
}

}  // namespace openjoey::lacooda64
