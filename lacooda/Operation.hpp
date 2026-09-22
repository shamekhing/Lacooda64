#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Operation word: opcode + subcode + flags + cause + aux.

#include <cstdint>
#include <type_traits>

#include "Word.hpp"
#include "Opcode.hpp"

namespace openjoey::lacooda64
{

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
    inline constexpr Word OP_BITS = 8;
    inline constexpr Word SUB_BITS = 12;
    inline constexpr Word FLAGS_BITS = 12;
    inline constexpr Word CAUSE_BITS = 8;
    inline constexpr Word AUX_BITS = 16;

    inline constexpr Word RESERVED_OP_BITS = 4;
    // Shifts accumulate from the low end so each field's shift = sum of lower widths.
    inline constexpr Word AUX_SHIFT = RESERVED_OP_BITS;
    inline constexpr Word CAUSE_SHIFT = AUX_SHIFT + AUX_BITS;
    inline constexpr Word FLAGS_SHIFT = CAUSE_SHIFT + CAUSE_BITS;
    inline constexpr Word SUB_SHIFT = FLAGS_SHIFT + FLAGS_BITS;
    inline constexpr Word OP_SHIFT = SUB_SHIFT + SUB_BITS;

    // Masks matching each field width.
    inline constexpr Word OP_MASK = (Word{1} << OP_BITS) - 1;
    inline constexpr Word SUB_MASK = (Word{1} << SUB_BITS) - 1;
    inline constexpr Word FLAGS_MASK = (Word{1} << FLAGS_BITS) - 1;
    inline constexpr Word CAUSE_MASK = (Word{1} << CAUSE_BITS) - 1;
    inline constexpr Word AUX_MASK = (Word{1} << AUX_BITS) - 1;

    // Packs an opcode bundle into a single Operation-tagged Word. `subcode`, `flags`,
    // `cause` and `aux` occupy their respective payload fields.
    [[nodiscard]] constexpr OperationWord Op(
        Opcode opcode,
        Word subcode = 0,
        Word flags = Flag_None,
        CauseKind cause = CauseKind::Unspecified,
        Word aux = 0) noexcept
    {
        const Word p =
            ((static_cast<Word>(opcode) & OP_MASK) << OP_SHIFT) |
            ((subcode & SUB_MASK) << SUB_SHIFT) |
            ((flags & FLAGS_MASK) << FLAGS_SHIFT) |
            ((static_cast<Word>(cause) & CAUSE_MASK) << CAUSE_SHIFT) |
            ((aux & AUX_MASK) << AUX_SHIFT);
        return makeTagged(WordTag::Operation, p);
    }

    // Converts any enum value to its underlying Word, for use in subcode fields.
    template <typename E>
    [[nodiscard]] constexpr Word sub(E e) noexcept
    {
        static_assert(std::is_enum<E>::value, "sub() requires enum");
        return static_cast<Word>(e);
    }

    // [59..52] Decodes the Opcode from an operation word.
    [[nodiscard]] constexpr Opcode opcodeOf(OperationWord op) noexcept
    {
        return static_cast<Opcode>((payloadOf(op) >> OP_SHIFT) & OP_MASK);
    }
    // [51..40] Decodes the 12-bit subcode from an operation word.
    [[nodiscard]] constexpr Word subcodeOf(OperationWord op) noexcept
    {
        return (payloadOf(op) >> SUB_SHIFT) & SUB_MASK;
    }
    // [39..28] Decodes the instruction flags from an operation word.
    [[nodiscard]] constexpr Word flagsOf(OperationWord op) noexcept
    {
        return (payloadOf(op) >> FLAGS_SHIFT) & FLAGS_MASK;
    }
    // [27..20] Decodes the cause kind from an operation word.
    [[nodiscard]] constexpr CauseKind causeOf(OperationWord op) noexcept
    {
        return static_cast<CauseKind>((payloadOf(op) >> CAUSE_SHIFT) & CAUSE_MASK);
    }
    // [19..4] Decodes the compact aux word from an operation word.
    [[nodiscard]] constexpr Word auxOf(OperationWord op) noexcept
    {
        return (payloadOf(op) >> AUX_SHIFT) & AUX_MASK;
    }

    // Summon method+mode share the 12-bit subcode field: low 8 = method, next 4 = mode.
    // Encodes a SummonMethod (8 bits) and SummonMode (4 bits) into a subcode value.
    [[nodiscard]] constexpr Word summonSubcode(SummonMethod method,
                                               SummonMode mode) noexcept
    {
        return (static_cast<Word>(method) & 0xFF) |
               ((static_cast<Word>(mode) & 0xF) << 8);
    }
    // Decodes the SummonMethod from a summon subcode.
    [[nodiscard]] constexpr SummonMethod summonMethod(Word code) noexcept
    {
        return static_cast<SummonMethod>(code & 0xFF);
    }
    // Decodes the SummonMode from a summon subcode.
    [[nodiscard]] constexpr SummonMode summonMode(Word code) noexcept
    {
        return static_cast<SummonMode>((code >> 8) & 0xF);
    }

} // namespace openjoey::lacooda64
