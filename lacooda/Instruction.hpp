#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Fixed instruction = exactly four 64-bit words = 32 bytes.

#include <array>
#include <cstddef>

#include "Operation.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Fixed instruction = exactly four 64-bit words = 32 bytes
//
//   word index     0           1           2           3
//   +------------+-----------+-----------+-----------+
//   | operation  |    dst    |   src0    |   src1    |
//   +------------+-----------+-----------+-----------+
//        64           64          64          64      bits
//
// Serialized byte offsets: 0..7, 8..15, 16..23, 24..31.
// EncodeBytes writes each word least-significant byte first.
// Unused operands default to kNone. Jump targets are instruction indices,
// not word indices or byte offsets.
// -----------------------------------------------------------------------------

// A fixed instruction is exactly four 64-bit Words: [op, dst, src0, src1].
// Because Word is trivially copyable, an Instruction (std::array<Word,4>) is
// itself trivially copyable and laid out contiguously as 32 bytes.
using Instruction = std::array<Word, 4>;

// Logical slot index of each Word within an Instruction (see MakeInstruction).
enum InstructionField : std::size_t {
  kIOp = 0,    // [0] operation word: opcode + subcode + flags + cause + aux.
  kIDst = 1,   // [1] destination operand.
  kISrc0 = 2,  // [2] source operand 0.
  kISrc1 = 3,  // [3] source operand 1.
};

// Assembles a 4-Word instruction from its decoded slots. `dst`/`src0`/`src1`
// default to kNone (WordTag::kNone) for opcodes that ignore them.
[[nodiscard]] constexpr Instruction MakeInstruction(OperationWord op, Operand dst = kNone, Operand src0 = kNone, Operand src1 = kNone) noexcept { return {op, dst, src0, src1}; }

// [0] Returns the operation Word of instruction `i`.
[[nodiscard]] constexpr OperationWord Operation(const Instruction& i) noexcept { return i[kIOp]; }
// [1] Returns the destination operand of instruction `i`.
[[nodiscard]] constexpr Operand Dst(const Instruction& i) noexcept { return i[kIDst]; }
// [2] Returns source operand 0 of instruction `i`.
[[nodiscard]] constexpr Operand Src0(const Instruction& i) noexcept { return i[kISrc0]; }
// [3] Returns source operand 1 of instruction `i`.
[[nodiscard]] constexpr Operand Src1(const Instruction& i) noexcept { return i[kISrc1]; }

}  // namespace openjoey::lacooda64
