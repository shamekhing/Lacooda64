#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Control-register word and the control-state array.

#include <array>
#include <cstddef>

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Control-register word
// -----------------------------------------------------------------------------

// Control registers index the engine's global state. The numeric value doubles as
// the array index into a ControlState and as the payload of a Control operand.
enum class ControlReg : Word {
    Turn = 0,       // Current turn number.
    Phase,          // Current phase (e.g. Draw, Standby, Main).
    Step,           // Current sub-step within a phase.
    Chain,          // Current chain layer being resolved.
    Effect,         // Current effect / frame identifier.
    ProgramCounter, // Index into the trace of the next instruction to run.
    Count           // Sentinel: number of control registers (array bound).
};

// Builds a Control-tagged operand selecting one of the control registers.
[[nodiscard]] constexpr Operand Control(ControlReg r) noexcept {
    return makeTagged(WordTag::Control, static_cast<Word>(r));
}

// Decodes which control register a Control-tagged operand selects.
[[nodiscard]] constexpr ControlReg controlReg(Operand w) noexcept {
    return static_cast<ControlReg>(payloadOf(w));
}

// True when `w` is a Control operand.
[[nodiscard]] constexpr bool isControl(Operand w) noexcept {
    return isTag(w, WordTag::Control);
}

// All control values are Words; ControlState is the fixed register file sized
// to exactly ControlReg::Count entries.
using ControlState = std::array<Word, static_cast<std::size_t>(ControlReg::Count)>;

// Maps a ControlReg to its index within a ControlState array.
[[nodiscard]] constexpr std::size_t controlIndex(ControlReg r) noexcept {
    return static_cast<std::size_t>(r);
}

} // namespace openjoey::lacooda64
