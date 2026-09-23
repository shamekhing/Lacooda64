#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// VM register files, machine state and bytecode encode/decode.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Control.hpp"
#include "Instruction.hpp"
#include "Validate.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// VM register files — all cells are 64-bit words
// -----------------------------------------------------------------------------

// The VM's three register files. Each holds 256 Words; the active bank is
// implicit in the operand tag/register-bank, so all three arrays are indexed by
// the same 8-bit register index.
struct Registers {
  std::array<Word, 256> value{};    // normally Immediate-tagged words
  std::array<Word, 256> address{};  // Address-tagged words
  std::array<Word, 256> flag{};     // Immediate 0/1 words
};

// Snapshot of all mutable machine state: the control register file and the
// three general register files.
struct MachineState {
  ControlState control{};
  Registers regs{};
};

// -----------------------------------------------------------------------------
// Bytecode
//
// A program is already a stream of 64-bit words.
// Each instruction occupies exactly four words.
// -----------------------------------------------------------------------------

// A serialised program: a flat vector of Words, four per instruction.
using ProgramWords = std::vector<Word>;

// Encodes a Trace (vector of Instructions) into a flat Word stream (4 Words
// each).
[[nodiscard]] inline ProgramWords Encode(const Trace& trace) {
  ProgramWords out;
  out.reserve(trace.size() * 4);
  for (const auto& i : trace) {
    out.insert(out.end(), i.begin(), i.end());
  }
  return out;
}

// Errors that can occur while decoding Words back into Instructions.
enum class DecodeError : Word {
  kNone = 0,
  kMisalignedWordCount,  // The word count was not a multiple of 4.
  kInvalidInstruction,   // A decoded instruction failed validation.
};

// Result of Decode(): the recovered Trace plus an error code/offset if any.
struct DecodeResult {
  Trace trace_{};
  DecodeError error_{DecodeError::kNone};
  Word word_offset_{0};  // Word index where the error occurred.

  [[nodiscard]] constexpr bool ok() const noexcept {
    return error_ == DecodeError::kNone;
  }
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return ok();
  }
};

// Decodes a flat Word stream into a Trace, validating each 4-Word instruction.
[[nodiscard]] inline DecodeResult Decode(const ProgramWords& words) noexcept {
  DecodeResult out{};
  if ((words.size() % 4) != 0) {
    out.error_ = DecodeError::kMisalignedWordCount;
    out.word_offset_ = static_cast<Word>(words.size());
    return out;
  }

  out.trace_.reserve(words.size() / 4);
  for (std::size_t i = 0; i < words.size(); i += 4) {
    Instruction inst{words[i], words[i + 1], words[i + 2], words[i + 3]};
    if (!ValidInstruction(inst)) {
      out.error_ = DecodeError::kInvalidInstruction;
      out.word_offset_ = static_cast<Word>(i);
      return out;
    }
    out.trace_.push_back(inst);
  }
  return out;
}

// Raw little-endian byte serialization, always 32 bytes/instruction.
using Bytecode = std::vector<std::uint8_t>;

// Appends `w` to `out` as 8 little-endian bytes.
inline void WriteU64Le(Bytecode& out, Word w) {
  for (unsigned i = 0; i < 8; ++i)
    out.push_back(static_cast<std::uint8_t>((w >> (i * 8)) & 0xFF));
}

// Encodes a Trace into a raw little-endian byte buffer (32 bytes per
// instruction).
[[nodiscard]] inline Bytecode EncodeBytes(const Trace& trace) {
  Bytecode out;
  out.reserve(trace.size() * 32);
  for (const auto& inst : trace)
    for (Word w : inst) WriteU64Le(out, w);
  return out;
}

}  // namespace openjoey::lacooda64
