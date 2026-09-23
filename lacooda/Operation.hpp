#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Operation word: opcode + subcode + flags + cause + aux.

#include <cstdint>
#include <type_traits>

#include "Opcode.hpp"
#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Operation word
//
// Full word:
//   63..60 tag = Operation
//
// Payload:
//   59..52 opcode   (8)
//   51..40 subcode  (12)
//   39..28 flags    (12)
//   27..20 cause    (8)
//   19.. 4 aux      (16)
//    3.. 0 reserved (4)
//
// `aux` is compact opcode-specific metadata.
// Larger values belong in normal operand words.
// -----------------------------------------------------------------------------

// Field widths of the operation-word payload. Together they occupy the low 60
// payload bits (8+12+12+8+16 = 56, plus 4 reserved = 60).
inline constexpr Word kOpBits = 8;
inline constexpr Word kSubBits = 12;
inline constexpr Word kFlagsBits = 12;
inline constexpr Word kCauseBits = 8;
inline constexpr Word kAuxBits = 16;

inline constexpr Word kReservedOpBits = 4;
// Shifts accumulate from the low end so each field's shift = sum of lower
// widths.
inline constexpr Word kAuxShift = kReservedOpBits;
inline constexpr Word kCauseShift = kAuxShift + kAuxBits;
inline constexpr Word kFlagsShift = kCauseShift + kCauseBits;
inline constexpr Word kSubShift = kFlagsShift + kFlagsBits;
inline constexpr Word kOpShift = kSubShift + kSubBits;

// Masks matching each field width.
inline constexpr Word kOpMask = (Word{1} << kOpBits) - 1;
inline constexpr Word kSubMask = (Word{1} << kSubBits) - 1;
inline constexpr Word kFlagsMask = (Word{1} << kFlagsBits) - 1;
inline constexpr Word kCauseMask = (Word{1} << kCauseBits) - 1;
inline constexpr Word kAuxMask = (Word{1} << kAuxBits) - 1;

// Packs an opcode bundle into a single Operation-tagged Word. `subcode`,
// `flags`, `cause` and `aux` occupy their respective payload fields.
[[nodiscard]] constexpr OperationWord Op(
    Opcode opcode, Word subcode = 0, Word flags = kFlagNone,
    CauseKind cause = CauseKind::kUnspecified, Word aux = 0) noexcept {
  const Word p = ((static_cast<Word>(opcode) & kOpMask) << kOpShift) |
                 ((subcode & kSubMask) << kSubShift) |
                 ((flags & kFlagsMask) << kFlagsShift) |
                 ((static_cast<Word>(cause) & kCauseMask) << kCauseShift) |
                 ((aux & kAuxMask) << kAuxShift);
  return MakeTagged(WordTag::kOperation, p);
}

// Converts any enum value to its underlying Word, for use in subcode fields.
template <typename E>
[[nodiscard]] constexpr Word Sub(E e) noexcept {
  static_assert(std::is_enum_v<E>, "Sub() requires enum");
  return static_cast<Word>(e);
}

// [59..52] Decodes the Opcode from an operation word.
[[nodiscard]] constexpr Opcode OpcodeOf(OperationWord op) noexcept {
  return static_cast<Opcode>((PayloadOf(op) >> kOpShift) & kOpMask);
}
// [51..40] Decodes the 12-bit subcode from an operation word.
[[nodiscard]] constexpr Word SubcodeOf(OperationWord op) noexcept {
  return (PayloadOf(op) >> kSubShift) & kSubMask;
}
// [39..28] Decodes the instruction flags from an operation word.
[[nodiscard]] constexpr Word FlagsOf(OperationWord op) noexcept {
  return (PayloadOf(op) >> kFlagsShift) & kFlagsMask;
}
// [27..20] Decodes the cause kind from an operation word.
[[nodiscard]] constexpr CauseKind CauseOf(OperationWord op) noexcept {
  return static_cast<CauseKind>((PayloadOf(op) >> kCauseShift) & kCauseMask);
}
// [19..4] Decodes the compact aux word from an operation word.
[[nodiscard]] constexpr Word AuxOf(OperationWord op) noexcept {
  return (PayloadOf(op) >> kAuxShift) & kAuxMask;
}

// Summon method+mode share the 12-bit subcode field: low 8 = method, next 4
// = mode. Encodes a SummonMethod (8 bits) and SummonMode (4 bits) into a
// subcode value.
[[nodiscard]] constexpr Word SummonSubcode(SummonMethod method,
                                           SummonMode mode) noexcept {
  return (static_cast<Word>(method) & 0xFF) |
         ((static_cast<Word>(mode) & 0xF) << 8);
}
// Decodes the SummonMethod from a summon subcode.
[[nodiscard]] constexpr SummonMethod SummonMethodOf(Word code) noexcept {
  return static_cast<SummonMethod>(code & 0xFF);
}
// Decodes the SummonMode from a summon subcode.
[[nodiscard]] constexpr SummonMode SummonModeOf(Word code) noexcept {
  return static_cast<SummonMode>((code >> 8) & 0xF);
}

}  // namespace openjoey::lacooda64
