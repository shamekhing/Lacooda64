#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Immediate word: signed 60-bit two's-complement payload.

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Immediate word
//
// Signed 60-bit two's-complement payload.
// Range: [-2^59, 2^59-1].
// -----------------------------------------------------------------------------

// An Immediate occupies the full 60-bit payload as a signed two's-complement
// value, giving a symmetric-ish range of [-2^59, 2^59-1].
inline constexpr SignedWord kImmediateMin = -(SignedWord{1} << 59);
inline constexpr SignedWord kImmediateMax = (SignedWord{1} << 59) - 1;

// Wraps a signed value into an Immediate-tagged operand. The payload is masked
// to 60 bits, so callers must keep `v` within [kImmediateMin, kImmediateMax].
[[nodiscard]] constexpr Operand Imm(SignedWord v) noexcept {
  return MakeTagged(WordTag::kImmediate, static_cast<Word>(v) & kPayloadMask);
}

// Sign-extends the 60-bit payload of an Immediate operand back to a full
// 64-bit signed value. Assumes `w` is already Immediate-tagged.
[[nodiscard]] constexpr SignedWord ImmediateValue(Operand w) noexcept {
  Word p = PayloadOf(w);
  constexpr Word sign = Word{1} << 59;
  if (p & sign) {
    p |= ~kPayloadMask;  // extend the sign bit into the unused high bits
  }
  return static_cast<SignedWord>(p);
}

// True when `w` is an Immediate operand.
[[nodiscard]] constexpr bool IsImmediate(Operand w) noexcept {
  return IsTag(w, WordTag::kImmediate);
}

}  // namespace openjoey::lacooda64
