#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Fixed instruction = exactly four 64-bit words = 32 bytes.

#include <array>
#include <cstddef>

#include "Word.hpp"
#include "Operation.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Fixed instruction = exactly four 64-bit words = 32 bytes
// -----------------------------------------------------------------------------

// A fixed instruction is exactly four 64-bit Words: [op, dst, src0, src1].
// Because Word is trivially copyable, an Instruction (std::array<Word,4>) is
// itself trivially copyable and laid out contiguously as 32 bytes.
using Instruction = std::array<Word, 4>;

// Logical slot index of each Word within an Instruction (see makeInstruction).
enum InstructionField : std::size_t {
    I_Op   = 0, // [0] operation word: opcode + subcode + flags + cause + aux.
    I_Dst  = 1, // [1] destination operand.
    I_Src0 = 2, // [2] source operand 0.
    I_Src1 = 3, // [3] source operand 1.
};

// Assembles a 4-Word instruction from its decoded slots. `dst`/`src0`/`src1`
// default to None (WordTag::None) for opcodes that ignore them.
[[nodiscard]] constexpr Instruction makeInstruction(
    OperationWord op,
    Operand dst = None,
    Operand src0 = None,
    Operand src1 = None
) noexcept {
    return {op, dst, src0, src1};
}

// [0] Returns the operation Word of instruction `i`.
[[nodiscard]] constexpr OperationWord operation(const Instruction& i) noexcept {
    return i[I_Op];
}
// [1] Returns the destination operand of instruction `i`.
[[nodiscard]] constexpr Operand dst(const Instruction& i) noexcept {
    return i[I_Dst];
}
// [2] Returns source operand 0 of instruction `i`.
[[nodiscard]] constexpr Operand src0(const Instruction& i) noexcept {
    return i[I_Src0];
}
// [3] Returns source operand 1 of instruction `i`.
[[nodiscard]] constexpr Operand src1(const Instruction& i) noexcept {
    return i[I_Src1];
}

} // namespace openjoey::lacooda64
