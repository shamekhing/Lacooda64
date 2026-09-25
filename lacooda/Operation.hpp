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
// Payload bits (56 used, 4 reserved):
//
//   payload bit (after removing the low tag with PayloadOf)
//   59..56 55..40 39..32 31..20 19..8  7..0
//   +------+------+------+-----+------+--------+
//   | rsvd | aux  |cause |flags| sub  | opcode |
//   +------+------+------+-----+------+--------+
//       4     16      8    12     12       8
//
// Full 64-bit word:
//   3..0 = WordTag::kOperation
//
// Summon subcode: bits 11..8 = mode, bits 7..0 = method.
// Op masks each field to its width and emits zero reserved bits.
//
// `aux` is compact opcode-specific metadata.
// Larger values belong in normal operand words.
// -----------------------------------------------------------------------------

// Field widths of the normalized operation payload. Together they occupy 60
// payload bits (8+12+12+8+16 = 56, plus 4 reserved = 60).
inline constexpr Word kOpBits = 8;
inline constexpr Word kSubBits = 12;
inline constexpr Word kFlagsBits = 12;
inline constexpr Word kCauseBits = 8;
inline constexpr Word kAuxBits = 16;

static_assert(kOpBits > 0 && kSubBits > 0 && kFlagsBits > 0 && kCauseBits > 0 && kAuxBits > 0, "Operation fields must have nonzero widths");
static_assert(kOpBits + kSubBits + kFlagsBits + kCauseBits + kAuxBits <= kPayloadBits, "Operation fields exceed the payload");
inline constexpr Word kReservedOpBits = kPayloadBits - (kOpBits + kSubBits + kFlagsBits + kCauseBits + kAuxBits);
// Shifts accumulate from the low end so each field's shift = sum of lower
// widths.
inline constexpr Word kOpShift = 0;
inline constexpr Word kSubShift = kOpShift + kOpBits;
inline constexpr Word kFlagsShift = kSubShift + kSubBits;
inline constexpr Word kCauseShift = kFlagsShift + kFlagsBits;
inline constexpr Word kAuxShift = kCauseShift + kCauseBits;
static_assert(kAuxShift + kAuxBits + kReservedOpBits == kPayloadBits);

inline constexpr Word kSummonMethodBits = 8;
static_assert(kSummonMethodBits > 0 && kSummonMethodBits < kSubBits, "Summon subcode must fit both method and mode");
inline constexpr Word kSummonModeBits = kSubBits - kSummonMethodBits;
inline constexpr Word kSummonMethodMask = (Word{1} << kSummonMethodBits) - 1;
inline constexpr Word kSummonModeMask = (Word{1} << kSummonModeBits) - 1;

// Masks matching each field width.
inline constexpr Word kOpMask = (Word{1} << kOpBits) - 1;
inline constexpr Word kSubMask = (Word{1} << kSubBits) - 1;
inline constexpr Word kFlagsMask = (Word{1} << kFlagsBits) - 1;
inline constexpr Word kCauseMask = (Word{1} << kCauseBits) - 1;
inline constexpr Word kAuxMask = (Word{1} << kAuxBits) - 1;

static_assert(static_cast<Word>(Opcode::kUnmodify) <= kOpMask, "Opcode width cannot represent all opcodes");
static_assert(static_cast<Word>(CauseKind::kReplacement) <= kCauseMask, "Cause width cannot represent all causes");
static_assert(kFlagMandatory <= kFlagsMask, "Flags width cannot represent all flags");
static_assert(static_cast<Word>(SummonMethod::kToken) <= kSummonMethodMask && static_cast<Word>(SummonMode::kSet) <= kSummonModeMask, "Summon widths cannot represent all variants");
static_assert(static_cast<Word>(EventKind::kDraw) <= kSubMask, "Subcode width cannot represent all events");

// Full sequential decode; individual accessors remain available below.
// Assumes an operation word. Validation is a separate step.
struct OperationFields {
  Opcode opcode;
  Word subcode;
  Word flags;
  CauseKind cause;
  Word aux;
};

[[nodiscard]] constexpr OperationFields DecodeOperation(OperationWord op) noexcept {
  Word cursor = PayloadOf(op);
  const auto opcode = static_cast<Opcode>(TakeField<kOpBits>(cursor));
  const auto subcode = TakeField<kSubBits>(cursor);
  const auto flags = TakeField<kFlagsBits>(cursor);
  const auto cause = static_cast<CauseKind>(TakeField<kCauseBits>(cursor));
  const auto aux = TakeField<kAuxBits>(cursor);
  return {opcode, subcode, flags, cause, aux};
}

// Packs an opcode bundle into a single Operation-tagged Word. `subcode`,
// `flags`, `cause` and `aux` occupy their respective payload fields.
[[nodiscard]] constexpr OperationWord Op(Opcode opcode, Word subcode = 0, Word flags = kFlagNone, CauseKind cause = CauseKind::kUnspecified, Word aux = 0) noexcept {
  const Word p = ((static_cast<Word>(opcode) & kOpMask) << kOpShift) | ((subcode & kSubMask) << kSubShift) | ((flags & kFlagsMask) << kFlagsShift) | ((static_cast<Word>(cause) & kCauseMask) << kCauseShift) | ((aux & kAuxMask) << kAuxShift);
  return MakeTagged(WordTag::kOperation, p);
}

// Converts any enum value to its underlying Word, for use in subcode fields.
template <typename E>
[[nodiscard]] constexpr Word Sub(E e) noexcept {
  static_assert(std::is_enum_v<E>, "Sub() requires enum");
  return static_cast<Word>(e);
}

// [payload 7..0] Decodes the Opcode from an operation word.
[[nodiscard]] constexpr Opcode OpcodeOf(OperationWord op) noexcept { return static_cast<Opcode>((PayloadOf(op) >> kOpShift) & kOpMask); }
// [payload 19..8] Decodes the 12-bit subcode from an operation word.
[[nodiscard]] constexpr Word SubcodeOf(OperationWord op) noexcept { return (PayloadOf(op) >> kSubShift) & kSubMask; }
// [payload 31..20] Decodes the instruction flags from an operation word.
[[nodiscard]] constexpr Word FlagsOf(OperationWord op) noexcept { return (PayloadOf(op) >> kFlagsShift) & kFlagsMask; }
// [payload 39..32] Decodes the cause kind from an operation word.
[[nodiscard]] constexpr CauseKind CauseOf(OperationWord op) noexcept { return static_cast<CauseKind>((PayloadOf(op) >> kCauseShift) & kCauseMask); }
// [payload 55..40] Decodes the compact aux word from an operation word.
[[nodiscard]] constexpr Word AuxOf(OperationWord op) noexcept { return (PayloadOf(op) >> kAuxShift) & kAuxMask; }

// Summon method+mode share the 12-bit subcode field: low 8 = method, next 4
// = mode. Encodes a SummonMethod (8 bits) and SummonMode (4 bits) into a
// subcode value.
[[nodiscard]] constexpr Word SummonSubcode(SummonMethod method, SummonMode mode) noexcept { return (static_cast<Word>(method) & kSummonMethodMask) | ((static_cast<Word>(mode) & kSummonModeMask) << kSummonMethodBits); }
// Decodes the SummonMethod from a summon subcode.
[[nodiscard]] constexpr SummonMethod SummonMethodOf(Word code) noexcept { return static_cast<SummonMethod>(code & kSummonMethodMask); }
// Decodes the SummonMode from a summon subcode.
[[nodiscard]] constexpr SummonMode SummonModeOf(Word code) noexcept { return static_cast<SummonMode>((code >> kSummonMethodBits) & kSummonModeMask); }

}  // namespace openjoey::lacooda64
