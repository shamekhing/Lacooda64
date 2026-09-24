#include <initializer_list>
#include <iostream>
#include <utility>

#include "Lacooda64.hpp"

using namespace openjoey::lacooda64;

namespace {
struct Field {
  Word value;
  Word width;
};

// Independent bit-by-bit reference: use widths, never production shifts,
// masks, packers, or accessors. Width changes should change expected words.
constexpr Word ReferenceWord(std::initializer_list<Field> fields) {
  Word result = 0;
  Word position = 0;
  for (auto field : fields) {
    for (Word bit = 0; bit < field.width; ++bit) {
      if (field.value & 1) result |= Word{1} << position;
      field.value >>= 1;
      ++position;
    }
  }
  return result;
}

constexpr Word ReferenceAddress(Word player, Word zone, Word slot, Word card,
                                Word attribute) {
  return ReferenceWord({{Sub(WordTag::kAddress), kTagBits},
                        {Sub(AddressLevel::kCard), kLevelBits},
                        {player, kPlayerBits},
                        {zone, kZoneBits},
                        {slot, kSlotBits},
                        {card, kCardBits},
                        {attribute, kAttributeBits}});
}

constexpr Word ReferenceOperation(Word opcode, Word subcode = 0, Word flags = 0,
                                  Word cause = 0, Word aux = 0) {
  return ReferenceWord({{Sub(WordTag::kOperation), kTagBits},
                        {opcode, kOpBits},
                        {subcode, kSubBits},
                        {flags, kFlagsBits},
                        {cause, kCauseBits},
                        {aux, kAuxBits}});
}

constexpr bool AddressChecks() {
  // Zero, single bits, all ones, and overflow in every field. Other fields
  // remain populated, exposing overlaps and accidental clearing of neighbors.
  for (Word bit = 0; bit <= kWordBits; ++bit) {
    const Word value = bit == kWordBits ? ~Word{0} : Word{1} << bit;
    const Word p = value, z = ~value, sl = value, c = ~value, a = value;
    const auto encoded = Card(p, z, sl, c, a);
    if (encoded != ReferenceAddress(p, z, sl, c, a)) return false;
    const auto fields = DecodeAddress(encoded);
    if (fields.level != AddressLevel::kCard ||
        LevelOf(encoded) != fields.level ||
        fields.player != (p & kPlayerMask) ||
        PlayerOf(encoded) != fields.player || fields.zone != (z & kZoneMask) ||
        ZoneOf(encoded) != fields.zone || fields.slot != (sl & kSlotMask) ||
        SlotOf(encoded) != fields.slot || fields.card != (c & kCardMask) ||
        CardOf(encoded) != fields.card ||
        fields.attribute != (a & kAttributeMask) ||
        AttributeOf(encoded) != fields.attribute)
      return false;
    if (WithAttribute(encoded, ~a) != ReferenceAddress(p, z, sl, c, ~a))
      return false;
    if (!IsObject(WithAttribute(encoded, kSelf))) return false;
  }
  return true;
}
static_assert(AddressChecks());
constexpr auto maximum_address =
    Card(kPlayerMask, kZoneMask, kSlotMask, kCardMask, kAttributeMask);
static_assert(maximum_address == ReferenceAddress(~Word{0}, ~Word{0}, ~Word{0},
                                                  ~Word{0}, ~Word{0}));
static_assert(PlayerOf(Player(kPlayerMask)) == kPlayerMask);
static_assert(PlayerOf(Player(kPlayerMask + 1)) == 0);
static_assert(ValidAddress(Card(0, 0, 0, 0)));
static_assert(!ValidAddress(
    MakeAddress(static_cast<AddressLevel>(Sub(AddressLevel::kCard) + 1))));

constexpr bool OperationChecks() {
  for (Word bit = 0; bit <= kWordBits; ++bit) {
    const Word value = bit == kWordBits ? ~Word{0} : Word{1} << bit;
    const auto encoded =
        Op(Opcode::kMove, value, ~value, CauseKind::kCardEffect, value);
    if (encoded != ReferenceOperation(Sub(Opcode::kMove), value, ~value,
                                      Sub(CauseKind::kCardEffect), value))
      return false;
    const auto fields = DecodeOperation(encoded);
    if (fields.opcode != Opcode::kMove || OpcodeOf(encoded) != fields.opcode ||
        fields.subcode != (value & kSubMask) ||
        SubcodeOf(encoded) != fields.subcode ||
        fields.flags != (~value & kFlagsMask) ||
        FlagsOf(encoded) != fields.flags ||
        fields.cause != CauseKind::kCardEffect ||
        CauseOf(encoded) != fields.cause || fields.aux != (value & kAuxMask) ||
        AuxOf(encoded) != fields.aux)
      return false;
    Word cursor = encoded;
    if (TakeField<kTagBits>(cursor) != Sub(WordTag::kOperation) ||
        TakeField<kOpBits>(cursor) != Sub(Opcode::kMove) ||
        TakeField<kSubBits>(cursor) != fields.subcode ||
        TakeField<kFlagsBits>(cursor) != fields.flags ||
        TakeField<kCauseBits>(cursor) != Sub(fields.cause) ||
        TakeField<kAuxBits>(cursor) != fields.aux || cursor != 0)
      return false;
  }
  return true;
}
static_assert(OperationChecks());

constexpr bool RegisterChecks() {
  for (Word bank = 0; bank <= Sub(RegisterBank::kFlag); ++bank) {
    for (Word bit = 0; bit <= kRegisterIndexBits; ++bit) {
      const Word index = Word{1} << bit;
      const auto encoded = Reg(static_cast<RegisterBank>(bank), index);
      if (encoded != ReferenceWord({{Sub(WordTag::kRegister), kTagBits},
                                    {bank, kRegisterBankBits},
                                    {index, kRegisterIndexBits}}))
        return false;
      const auto fields = DecodeRegister(encoded);
      if (Sub(fields.bank) != bank || RegisterBankOf(encoded) != fields.bank ||
          fields.index != (index & kRegisterIndexMask) ||
          RegisterIndexOf(encoded) != fields.index)
        return false;
    }
  }
  return true;
}
static_assert(RegisterChecks());
static_assert(V(kRegisterCount) == V(0));
static_assert(Registers{}.value.size() == kRegisterCount);
static_assert(Registers{}.address.size() == kRegisterCount);
static_assert(Registers{}.flag.size() == kRegisterCount);

constexpr bool ImmediateChecks() {
  for (const auto value : {SignedWord{0}, SignedWord{1}, SignedWord{-1},
                           kImmediateMin, kImmediateMax}) {
    const auto encoded = Imm(value);
    if (encoded != ReferenceWord({{Sub(WordTag::kImmediate), kTagBits},
                                  {static_cast<Word>(value), kPayloadBits}}) ||
        TagOf(encoded) != WordTag::kImmediate ||
        PayloadOf(encoded) != (static_cast<Word>(value) & kPayloadMask) ||
        ImmediateValue(encoded) != value)
      return false;
  }
  Word cursor = ~Word{0};
  return TakeField<kWordBits>(cursor) == ~Word{0} && cursor == 0;
}
static_assert(ImmediateChecks());
static_assert(ImmediateValue(Imm(kImmediateMax + 1)) == kImmediateMin);
static_assert(Imm(0) != kNone);
static_assert(ControlRegOf(Control(ControlReg::kProgramCounter)) ==
              ControlReg::kProgramCounter);
constexpr auto summon_code =
    SummonSubcode(SummonMethod::kSpecial, SummonMode::kFaceDownDefense);
static_assert(summon_code ==
              ReferenceWord({{Sub(SummonMethod::kSpecial), kSummonMethodBits},
                             {Sub(SummonMode::kFaceDownDefense),
                              kSummonModeBits}}));
static_assert(SummonMethodOf(summon_code) == SummonMethod::kSpecial);
static_assert(SummonModeOf(summon_code) == SummonMode::kFaceDownDefense);
static_assert(!ValidOperand(MakeTagged(WordTag::kNone, 1)));
static_assert(!ValidOperand(Reg(static_cast<RegisterBank>(3), 0)));
static_assert(!ValidOperand(Control(ControlReg::kCount)));
static_assert(!noexcept(Decode(std::declval<const ProgramWords&>())));
}  // namespace

int main() {
  const auto check = [](bool passed, const char* message) {
    if (!passed) std::cerr << message << '\n';
    return passed;
  };
  // Independent word construction and byte extraction pin order/endianness
  // without pinning configurable widths to an obsolete hexadecimal fixture.
  const Trace trace{Set(V(0), Imm(-1)), Halt()};
  const ProgramWords expected_words{
      ReferenceOperation(Sub(Opcode::kSet)),
      ReferenceWord({{Sub(WordTag::kRegister), kTagBits}}),
      ReferenceWord(
          {{Sub(WordTag::kImmediate), kTagBits}, {~Word{0}, kPayloadBits}}),
      0,
      ReferenceOperation(Sub(Opcode::kHalt)),
      0,
      0,
      0};
  Bytecode expected;
  for (auto word : expected_words) {
    for (unsigned byte = 0; byte < sizeof(Word); ++byte) {
      expected.push_back(static_cast<std::uint8_t>(word % 256));
      word /= 256;
    }
  }
  bool ok = check(Encode(trace) == expected_words, "Unexpected encoded words");
  ok &= check(EncodeBytes(trace) == expected, "Unexpected encoded bytes");
  const Trace effect{example::kCountMonsters, example::kCompareThree,
                     example::kJumpToEffect,  Halt(),
                     example::kDrawTwo,       Halt()};
  const auto decoded = Decode(Encode(effect));
  ok &= check(decoded.ok() && decoded.trace_ == effect &&
                  ValidateTrace(decoded.trace_).ok(),
              "Effect round trip failed");
  for (const auto jump :
       {Jump(2), JumpIf(F(0), 2), Jump(Word{1} << kPayloadBits),
        JumpIf(F(0), ~Word{0})}) {
    const auto result = ValidateTrace(Trace{jump, Halt()});
    ok &= check(result.error_ == ValidationError::kJumpOutOfRange &&
                    result.instruction_ == 0,
                "Branch validation failed");
  }
  ok &= check(ValidateTrace(Trace{Jump(1), Halt()}).ok(),
              "Valid branch rejected");
  const auto misaligned = Decode(ProgramWords{0});
  ok &= check(misaligned.error_ == DecodeError::kMisalignedWordCount &&
                  misaligned.word_offset_ == 1,
              "Alignment validation failed");
  auto invalid = Encode(trace);
  invalid[4] = kNone;
  const auto bad = Decode(invalid);
  ok &= check(bad.error_ == DecodeError::kInvalidInstruction &&
                  bad.word_offset_ == 4 && bad.trace_ == Trace{trace[0]},
              "Invalid instruction prefix handling failed");
  ok &=
      check(Decode({}).ok() && ValidateTrace({}).ok(), "Empty trace rejected");
  return ok ? 0 : 1;
}
