#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
//
// Umbrella header for the factored Lacooda ISA.
//
// The former monolithic 64-bit VM header has been split into the lacooda/
// modules below, included here in dependency (layer) order so each lower
// layer never depends on a higher one.
//
// Everything in the executable duel layer is represented as std::uint64_t:
//
//   Address       = Word
//   Operand       = Word
//   Register      = Word
//   Control value = Word
//   Operation     = Word
//   Instruction   = 4 x Word
//   Program       = Word[]
//
// Instruction layout:
//   [0] operation word = opcode + subcode + flags + cause + aux
//   [1] destination operand
//   [2] source operand 0
//   [3] source operand 1
//
// Operand layout:
//   63..60  tag
//   59..0   payload
//

#include <type_traits>

#include "lacooda/Address.hpp"
#include "lacooda/Builder.hpp"
#include "lacooda/Control.hpp"
#include "lacooda/Example.hpp"
#include "lacooda/Immediate.hpp"
#include "lacooda/Instruction.hpp"
#include "lacooda/Machine.hpp"
#include "lacooda/Opcode.hpp"
#include "lacooda/Operand.hpp"
#include "lacooda/Operation.hpp"
#include "lacooda/Register.hpp"
#include "lacooda/Validate.hpp"
#include "lacooda/Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Static guarantees
// -----------------------------------------------------------------------------

static_assert(sizeof(Word) == 8, "Word must be exactly 64 bits");
static_assert(sizeof(Instruction) == 32, "Instruction must be exactly 4 words");
static_assert(std::is_trivially_copyable<Instruction>::value,
              "Instruction must remain trivially copyable");

}  // namespace openjoey::lacooda64
