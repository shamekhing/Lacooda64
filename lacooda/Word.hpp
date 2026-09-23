#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Foundation: the 64-bit word type, type aliases and tagged-word helpers.

#include <cstdint>

namespace openjoey::lacooda64 {

using Word = std::uint64_t;
using SignedWord = std::int64_t;

using Address = Word;
using Operand = Word;
using OperationWord = Word;
using PlayerId = Word;
using ZoneId = Word;
using SlotId = Word;
using CardInstanceId = Word;
using CardCode = Word;
using AttributeId = Word;
using PhaseId = Word;
using StepId = Word;
using ChainId = Word;
using EffectId = Word;
using ProgramCounter = Word;
using RegisterIndex = Word;

inline constexpr Word kWordBits = 64;
inline constexpr Word kTagBits = 4;
inline constexpr Word kPayloadBits = kWordBits - kTagBits;
inline constexpr Word kPayloadMask = (Word{1} << kPayloadBits) - 1;
inline constexpr Word kTagShift = kPayloadBits;

// -----------------------------------------------------------------------------
// Tagged 64-bit words
// -----------------------------------------------------------------------------

// Tag stored in the top 4 bits of every Word (see kTagShift). It lets a single
// 64-bit value carry both a discriminated "kind" and its 60-bit payload, so
// operands for addresses, registers, immediates, controls and operations are
// all just Words at the machine level.
enum class WordTag : Word {
  kNone = 0x0,       // Uninitialised / empty-operand sentinel.
  kAddress = 0x1,    // Encodes a location in the duel (see Address.hpp).
  kRegister = 0x2,   // Encodes a register bank + index (see Register.hpp).
  kImmediate = 0x3,  // Encodes a signed 60-bit literal (see Immediate.hpp).
  kControl = 0x4,    // Encodes a control-register selector (see Control.hpp).
  kOperation = 0x5,  // Encodes an opcode bundle (see Opcode.hpp).
  kLabel = 0x6,      // Reserved for symbolic labels / relocation slots.
  kReserved7 = 0x7,  // Available for future tag kinds.
};

// Builds a tagged Word: places `tag` in the high nibble and masks `payload`
// into the low 60 bits. This is the canonical constructor for any operand.
[[nodiscard]] constexpr Word MakeTagged(WordTag tag,
                                        Word payload = 0) noexcept {
  return (static_cast<Word>(tag) << kTagShift) | (payload & kPayloadMask);
}

// Returns the high-nibble tag of a Word, i.e. its kind.
[[nodiscard]] constexpr WordTag TagOf(Word w) noexcept {
  return static_cast<WordTag>((w >> kTagShift) & 0xF);
}

// Extracts the 60-bit payload of a Word with the tag bits cleared.
[[nodiscard]] constexpr Word PayloadOf(Word w) noexcept {
  return w & kPayloadMask;
}

// True when `w` carries the given tag.
[[nodiscard]] constexpr bool IsTag(Word w, WordTag tag) noexcept {
  return TagOf(w) == tag;
}

// Canonical "no operand" value: a fully-zero WordTag::kNone word.
inline constexpr Word kNone = MakeTagged(WordTag::kNone, 0);

}  // namespace openjoey::lacooda64
