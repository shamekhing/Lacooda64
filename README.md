# Lacooda64

**A C++20 duel instruction set, runtime, assembler and effect compiler.**

![image info](./Lacooda64.png)


Lacooda64 gives OpenJoey2 a common representation for duel operations: moving a
card, changing life points, comparing values, branching, and recording events.
Addresses and operands fit into tagged 64-bit words. Every instruction occupies
four words—32 bytes—so a program is a sequence with fixed boundaries and explicit
operand types.

The core defines tagged words, instructions, builders, validation and serialization.
The runtime executes instructions and changes duel state through a storage adapter.
The assembler and source converter produce the same native instruction format.
These layers use the C++20 standard library; the converter uses Python 3.10+.

Public headers live in `include/`, implementations in `src/`. Compile-time packing,
builders and single-instruction checks remain header-visible. Serialization,
trace validation, runtime and assembly are compiled libraries. All targets are
defined in the root `CMakeLists.txt`; this README is the repository's sole manual.

```cpp
using namespace openjoey::lacooda64;

constexpr auto damage = Damage(Player(1), Imm(500));
constexpr auto draw = Move(Zone(0, 3), Zone(0, 2), MoveMethod::kDraw);
constexpr auto check = Compare(F(0), V(0), Imm(3), CompareOp::kGreaterEqual);

static_assert(ValidInstruction(damage));
```

These calls **construct instructions**. `Runtime::Run` executes supported instructions.
Encoding an operation does not imply its runtime semantics are implemented.
Zone IDs in this README are examples chosen by the host ruleset.

## Contents

- [Get started](#get-started)
- [Build a duel effect](#build-a-duel-effect)
- [Work with operands](#work-with-operands)
- [Instruction set](#instruction-set)
- [Validate and serialize](#validate-and-serialize)
- [Binary format](#binary-format)
- [Integrate an engine](#integrate-an-engine)
- [Source guide](#source-guide)
- [Repository layout](#repository-layout)
- [Runtime execution](#runtime-execution)
- [Assembly language](#assembly-language)
- [Effect corpus and bindings](#effect-corpus-and-bindings)
- [Command-line tools](#command-line-tools)
- [Executable runtime example](#executable-runtime-example)
- [Opcode and subcode spelling reference](#opcode-and-subcode-spelling-reference)
- [Public API contracts](#public-api-contracts)
- [Validation, testing and troubleshooting](#validation-testing-and-troubleshooting)
- [License](#license)

## Get started

Place the repository at `external/Lacooda64` in your project:

```text
my-engine/
├── CMakeLists.txt
├── main.cpp
└── external/
    └── Lacooda64/
        ├── CMakeLists.txt
        ├── include/
        │   ├── Lacooda64.hpp
        │   ├── lacooda/
        │   ├── runtime/
        │   └── assembly/
        └── src/
```

Use CMake 3.16 or later and a compiler supporting C++20:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyDuelEngine LANGUAGES CXX)

add_subdirectory(external/Lacooda64)
add_executable(duel main.cpp)
target_link_libraries(duel PRIVATE Lacooda64::Lacooda64)
```

The target supplies the include path and C++20 requirement. Save the next example
as `main.cpp`, then build and run it from `my-engine/`:

```sh
cmake -S . -B build
cmake --build build
./build/duel
```

With a multi-configuration generator, build with `--config Release` and run the
executable from the corresponding configuration directory.

The root build produces three static libraries. Link `Lacooda64::Core` for the
ISA and serialization, `Lacooda64::Runtime` for execution, and
`Lacooda64::Assembly` for text assembly. `Lacooda64::Lacooda64` aliases Core.
Runtime and Assembly propagate Core and the public include directory. They have
no dependency on one another. There are no installation or `find_package` rules.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DLACOODA64_BUILD_TESTS=ON -DLACOODA64_BUILD_TOOLS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Both options default to `OFF`; all three libraries are always built. Enabling
tests requires Python 3.10+ so conversion checks cannot silently disappear.
The CLI option builds `lacooda-asm`. Tests alone need the repository root for
fixture headers; consumers receive only `include/`.

Direct compilation of a Core consumer is also possible:

```sh
c++ -std=c++20 -Iexternal/Lacooda64/include main.cpp \
  external/Lacooda64/src/lacooda/Machine.cpp \
  external/Lacooda64/src/lacooda/Validate.cpp -o duel
```

Existing `#include "Lacooda64.hpp"` and `#include "lacooda/Address.hpp"` paths
are unchanged. Consumers that previously used only headers must now link Core
when calling serialization or trace validation. Templates and `constexpr` APIs
continue to support `static_assert` without moving their definitions out of view.

## Build a duel effect

Suppose an effect says: **if player 0 controls at least three monsters, draw two
cards**. Its instruction sequence needs to count, compare, branch, and move cards:

```text
 PC   Instruction                         Meaning
  0   COUNT    V0, P0.MONSTERS             Count the monsters into V0.
  1   COMPARE  F0, V0, #3, GREATER_EQUAL   Set F0 to the comparison result.
  2   JUMPIF   F0, 4                      Continue at instruction 4 if true.
  3   HALT                                Otherwise stop.
  4   MOVE     DRAW, P0.HAND, P0.DECK, #2  Draw two from deck into hand.
  5   HALT                                Stop after the draw.
```

This is explanatory assembly notation; the C++ builders create the actual
instructions. Here is a complete program that builds the effect, validates it,
and checks that encoding and decoding preserve every word:

```cpp
#include "Lacooda64.hpp"
#include <iostream>

int main() {
  using namespace openjoey::lacooda64;

  constexpr ZoneId monster_zone = 1;
  constexpr ZoneId deck_zone = 2;
  constexpr ZoneId hand_zone = 3;

  constexpr Address monsters = Zone(0, monster_zone);
  constexpr Address deck = Zone(0, deck_zone);
  constexpr Address hand = Zone(0, hand_zone);

  // Move() leaves the count operand empty. Specify src1 to encode draw-two.
  constexpr Instruction draw_two = MakeInstruction(
      Op(Opcode::kMove, Sub(MoveMethod::kDraw), kFlagNone,
         CauseKind::kCardEffect),
      hand, deck, Imm(2));

  const Trace effect{
      Count(V(0), monsters),
      Compare(F(0), V(0), Imm(3), CompareOp::kGreaterEqual),
      JumpIf(F(0), 4),
      Halt(),
      draw_two,
      Halt(),
  };

  const auto validation = ValidateTrace(effect);
  if (!validation) {
    std::cerr << "Validation error " << static_cast<Word>(validation.error_)
              << " at instruction " << validation.instruction_ << '\n';
    return 1;
  }

  const ProgramWords words = Encode(effect);
  const Bytecode bytes = EncodeBytes(effect);
  const DecodeResult decoded = Decode(words);
  if (!decoded) {
    std::cerr << "Decode error at word " << decoded.word_offset_ << '\n';
    return 2;
  }
  if (!ValidateTrace(decoded.trace_) || decoded.trace_ != effect)
    return 3;

  std::cout << effect.size() << " instructions, " << words.size()
            << " words, " << bytes.size() << " bytes\n";
}
```

Output:

```text
6 instructions, 24 words, 192 bytes
```

`Trace` is a vector of instructions. `V(0)` names a value register, `F(0)` names a
flag register, and `Imm(3)` holds a literal. `JumpIf(F(0), 4)` refers to the fifth
instruction: program counters are **zero-based instruction indices**, not word
or byte offsets. The program above constructs and serializes the effect; an
interpreter would carry out the count and draw.

## Work with operands

All examples below assume `#include "Lacooda64.hpp"` and
`using namespace openjoey::lacooda64;`. Each snippet is independent.

### Address an object or one of its attributes

Addresses describe a path through the duel:

```text
Duel → Player → Zone → Slot → Card instance
```

Every depth can select either the object itself or one of its attributes.
Attribute `kSelf` (zero) means the whole object. Other IDs are defined by your
ruleset—for example, life points on a player or attack on a card.

```cpp
constexpr AttributeId life_points = 2;
constexpr AttributeId attack = 1;
constexpr ZoneId monster_zone = 1;

constexpr Address player = Player(0);
constexpr Address lp = Player(0, life_points);
constexpr Address card = Card(0, monster_zone, 2, 1234);
constexpr Address atk = WithAttribute(card, attack);

static_assert(IsObject(player));
static_assert(IsAttribute(lp));
static_assert(CardOf(atk) == 1234);
static_assert(AttributeOf(atk) == attack);
```

| Constructor | Path | Selects |
| --- | --- | --- |
| `Duel(a = kSelf)` | `[a]` | Duel or duel attribute |
| `Player(p, a = kSelf)` | `[p:a]` | Player or player attribute |
| `Zone(p, z, a = kSelf)` | `[p:z:a]` | Zone or zone attribute |
| `Slot(p, z, sl, a = kSelf)` | `[p:z:sl:a]` | Slot or slot attribute |
| `Card(p, z, sl, c, a = kSelf)` | `[p:z:sl:c:a]` | Card instance or card attribute |

A card-instance ID identifies the instance in that location. The host determines
how instances are allocated and how addresses change when cards move.

### Load, calculate, and store a value

Use value registers for intermediate calculations and an attribute address for
state that belongs to the duel:

```cpp
constexpr Address lp = Player(0, 2);  // Attribute 2 means LP in this ruleset.

const Trace adjustment{
    Load(V(0), lp),
    Alu(V(1), V(0), Imm(500), AluOp::kSubtract),
    Store(lp, V(1)),
    Halt(),
};
```

This expresses a read/modify/write sequence. Use the domain-specific builders
when the instruction should retain a game meaning: `Damage(Player(0), Imm(500))`
records damage, while `PayLp(Player(0), Imm(500))` records a cost payment.
`PayLp` sets `kFlagCost` by default.

### Keep an address in a register

There are three banks of 256 registers:

| Selector | Bank | Intended use |
| --- | --- | --- |
| `V(0)` … `V(255)` | Value | Values and intermediate calculations |
| `A(0)` … `A(255)` | Address | References to duel locations |
| `F(0)` … `F(255)` | Flag | Boolean results and branch conditions |

```cpp
constexpr Address target = Card(1, 1, 0, 42);
const Trace position_change{
    Select(A(0), target),
    Position(A(0), PositionOp::kFaceUpDefense),
    Halt(),
};
```

An address register can be used where an opcode accepts an *address source*.
Some opcodes require a literal address: `Count`, `Select`, and `Move`, for
example. The [operand rules](#operand-rules) below specify the difference.

### Record an outcome or an event

A resolved die roll can be stored in a trace together with its domain:

```cpp
constexpr auto roll = RecordedRandom(V(0), RandomKind::kDie, Imm(4), Imm(6));
constexpr auto event = Event(EventKind::kDieRolled, Player(0), Imm(4));
static_assert(ValidInstruction(roll));
static_assert(ValidInstruction(event));
```

`RecordedRandom` puts the resolved result in `src0` and optional domain/context in
`src1`; the runtime writes the result into `dst` when executing it. Event operands
represent subject, object, and context. Their meaning is agreed with the engine.

## Instruction set

The builders in [Builder.hpp](include/lacooda/Builder.hpp) cover every opcode family.
They return an `Instruction` and can be used in constant expressions. The table
shows their intended engine operations; builders themselves only pack words.

| Area | Builders | Purpose |
| --- | --- | --- |
| Values | `Set`, `Copy`, `Load`, `Store`, `Swap` | Assign, read, write, or exchange values |
| Locations | `Select`, `Count` | Retain an address or count a container's contents |
| Movement | `Move`, `Summon`, `Position` | Move cards, summon them, or change position |
| Effects | `Equip`, `Counter`, `ChangeControl`, `Negate`, `Restrict` | Describe changes to cards and play restrictions |
| Arithmetic | `Alu`, `AluUnary`, `Compare` | Calculate values and produce comparison flags |
| Flow | `Nop`, `Jump`, `JumpIf`, `Halt` | Control instruction execution |
| Life points | `Damage`, `GainLp`, `PayLp` | Preserve the reason for an LP change |
| Outcomes | `RecordedRandom`, `Random` | Record an outcome or request seeded generation |
| Collections | `Enumerate`, `At`, `Append`, `Length`, `Attribute` | Traverse objects and address properties |
| Decisions | `Choose` | Suspend for an indexed player decision |
| Lifetimes | `Schedule`, `Subscribe`, `Cancel` | Capture and later resume instruction bodies |
| Modifiers | `Modify`, `Unmodify` | Compose and remove numeric contributions |
| History | `History` | Read recorded event fields |
| Replacements | `Stage`, `Commit` | Amend or cancel a pending mutation |
| Protocol | `Event`, `Chain` | Record events and chain-resolution steps |

### Variants and metadata

An operation carries an opcode, subcode, flags, cause, and a compact auxiliary
value. Subcodes select variants such as `MoveMethod::kDraw`, `AluOp::kAdd`, or
`CompareOp::kGreaterEqual`. [Opcode.hpp](include/lacooda/Opcode.hpp) defines the enums.

Summon packs both a method and a mode into its subcode:

```cpp
constexpr auto summon = Summon(
    Slot(0, 1, 0), Card(0, 3, 0, 42),
    SummonMethod::kNormal, SummonMode::kFaceUpAttack);

static_assert(ValidInstruction(summon));
static_assert(SummonMethodOf(SubcodeOf(Operation(summon))) ==
              SummonMethod::kNormal);
```

Flags combine with `|`: `kFlagForced`, `kFlagOptional`, `kFlagCost`,
`kFlagReplacement`, `kFlagPublic`, `kFlagHidden`, and `kFlagMandatory`.
Causes distinguish rules, player actions, card effects, battle, summon procedures,
maintenance, and replacement effects, with `kUnspecified` as the default for `Op`.

Use `MakeInstruction` when you need explicit control over all four slots:

```cpp
constexpr auto draw_two = MakeInstruction(
    Op(Opcode::kMove, Sub(MoveMethod::kDraw), kFlagPublic,
       CauseKind::kCardEffect),
    Zone(0, 3), Zone(0, 2), Imm(2));

static_assert(OpcodeOf(Operation(draw_two)) == Opcode::kMove);
static_assert(ImmediateValue(Src1(draw_two)) == 2);
```

`Op` takes arguments in the order **opcode, subcode, flags, cause, aux**. The
`Move` builder takes **destination, source, method, cause, flags**. Supplying
flags to a builder replaces its default flags; it does not add to them.
`Control(...)` constructs a control-register operand, while `ChangeControl(...)`
constructs a card-control instruction.

## Validate and serialize

A normal producer builds a trace, validates it, and encodes it. A consumer decodes
the words, checks the result, and validates branch targets before execution:

```text
instructions → ValidateTrace → Encode      → 64-bit words
                            ↘ EncodeBytes → little-endian bytes

64-bit words → Decode → ValidateTrace → Runtime::Run
```

### Check instructions and branches

`ValidOperand` checks an individual operand. `ValidInstruction` checks an
instruction's tags, opcode, and operand rules. `ValidateTrace` checks every
instruction and the bounds of each `Jump`/`JumpIf` target.

```cpp
const Trace bad_branch{Jump(2), Halt()};  // Only indices 0 and 1 exist.
const auto result = ValidateTrace(bad_branch);
// result.error_ == ValidationError::kJumpOutOfRange
// result.instruction_ == 0
```

`ValidationResult` supports `ok()` and explicit boolean conversion. On failure,
`instruction_` identifies the first offending instruction:

| `ValidationError` | Meaning |
| --- | --- |
| `kNone` | Validation succeeded |
| `kInvalidInstruction` | An instruction failed its structural checks |
| `kJumpOutOfRange` | A target is negative or outside the trace |

Jump builders encode targets above `kImmediateMax` as an invalid negative target,
so validation rejects them instead of accepting a truncated PC.

An empty trace is valid. Validation does not require a Halt or establish that the
program terminates. It checks representation and operand structure; the engine
checks game legality and whether addressed objects exist.

### Choose words or bytes

| Call | Result | Layout |
| --- | --- | --- |
| `Encode(trace)` | `ProgramWords` (`std::vector<Word>`) | Four numerical words per instruction |
| `EncodeBytes(trace)` | `Bytecode` (`std::vector<std::uint8_t>`) | 32 bytes per instruction, little-endian |
| `Decode(words)` | `DecodeResult` | Reconstructed instructions with structural checks |

Neither encoder validates the input. Use `EncodeBytes` for a defined byte order;
writing the native memory of `ProgramWords` would use the host's byte order.

`Decode` checks instruction boundaries and individual instructions. It leaves
branch-range checks to `ValidateTrace`. Its result contains `trace_`, `error_`,
`word_offset_`, `ok()`, and explicit boolean conversion:

| `DecodeError` | `word_offset_` | `trace_` |
| --- | --- | --- |
| `kNone` | 0 | Complete decoded trace |
| `kMisalignedWordCount` | Input word count | Empty |
| `kInvalidInstruction` | First word of the failing instruction | Valid prefix before the failure |

A failed decode may return a partial trace, so check the result before using it.
`Decode` allocates; allocation exceptions propagate to the caller.

The library currently has no byte-buffer decoder. A host reading bytes must check
that the length is a multiple of 32, reconstruct words from groups of eight
little-endian bytes, and then call `Decode` and `ValidateTrace`. A file or network
protocol must supply its own framing, version, and ruleset identification.

## Binary format

Bit ranges below are inclusive. Bit 0 is the least-significant bit. Each encoded
word has a four-bit tag in bits 3..0 and a 60-bit payload in bits 63..4.
Payload diagrams use normalized positions 59..0, after removing the tag.

```text
 63..4                                                        3..0
+------------------------------------------------------------+------+
| payload                                                    | tag  |
+------------------------------------------------------------+------+
                             60                                  4
```

`Word` is `std::uint64_t`; `SignedWord` is `std::int64_t`. `Address`, `Operand`,
`OperationWord`, and the identifier types are aliases of `Word`. Tags distinguish
encoded values at runtime; the aliases do not introduce separate C++ types.

### Sequential decoding

Read from the least-significant end: take a field, shift it away, and continue.
`TakeField<Width>(cursor)` masks and consumes exactly the named width. Widths
are compile-time constants; values from 1 through 64 are supported.

```cpp
Word cursor = Op(Opcode::kMove, Sub(MoveMethod::kDraw), kFlagPublic,
                 CauseKind::kCardEffect, 42);
const auto tag = static_cast<WordTag>(TakeField<kTagBits>(cursor));
const auto opcode = static_cast<Opcode>(TakeField<kOpBits>(cursor));
const auto subcode = TakeField<kSubBits>(cursor);
const auto flags = TakeField<kFlagsBits>(cursor);
const auto cause = static_cast<CauseKind>(TakeField<kCauseBits>(cursor));
const auto aux = TakeField<kAuxBits>(cursor);
// cursor now contains the four reserved bits.
```

The consumption order is:

| Kind | Fields after the tag |
| --- | --- |
| Operation | Opcode → subcode → flags → cause → aux → reserved |
| Address | Level → player → zone → slot → card instance → attribute → reserved |
| Register | Bank → index → reserved |
| Immediate | Signed 60-bit payload |
| Control | 60-bit selector |

`DecodeOperation`, `DecodeAddress`, and `DecodeRegister` return field structures
using this sequence. They assume the appropriate tag and do not validate the
word. `PayloadOf` removes the low tag and normalizes the remaining payload;
individual accessors use offsets cumulatively derived from the same widths.

```cpp
constexpr auto fields = DecodeOperation(
    Op(Opcode::kMove, Sub(MoveMethod::kDraw), kFlagPublic));
static_assert(fields.opcode == Opcode::kMove);
static_assert(fields.subcode == Sub(MoveMethod::kDraw));
static_assert(fields.flags == kFlagPublic);
```

**Encoding compatibility:** this layout replaces the former high-bit tag format.
Previously encoded words/bytes must be re-encoded from their original format;
there is no automatic legacy-format detection. Instruction size and little-endian
byte order remain unchanged.

### Changing field widths

The `k*Bits` constants are the layout definitions. Shifts and masks are derived
from them, and address/operation reserved space is the unused payload remainder.
Compile-time checks reject fields that exceed the payload or cannot represent
supported enum values. Register storage is sized from `kRegisterIndexBits`.

Encoding tests use an independent bit-by-bit reference driven by those widths,
including boundary and overflow cases. They do not require updating hexadecimal
fixtures after a valid width change. The diagrams and numeric ranges here describe
the current configuration and must be updated when changing it. All producers and
consumers must use the same widths; stored bytecode from another layout needs
migration.

### Tags

| Tag | Value | Payload |
| --- | --- | --- |
| `WordTag::kNone` | `0x0` | Empty operand; the canonical word is zero |
| `WordTag::kAddress` | `0x1` | Hierarchical duel address |
| `WordTag::kRegister` | `0x2` | Bank and register index |
| `WordTag::kImmediate` | `0x3` | Signed 60-bit literal |
| `WordTag::kControl` | `0x4` | Control-register selector |
| `WordTag::kOperation` | `0x5` | Opcode and instruction metadata |
| `WordTag::kLabel` | `0x6` | Reserved for symbolic labels/relocations |
| `WordTag::kCollection` | `0x7` | Invocation-local runtime collection handle |

`MakeTagged`, `TagOf`, `PayloadOf`, and `IsTag` construct and inspect this outer
layout. Operation words belong in instruction slot 0. Label and reserved tags
are not currently valid operands.

### Address word

**Tag:** `WordTag::kAddress` · **Payload:** 56 used bits, 4 reserved

```text
 59..56 55..44     43..20     19..14 13..8  7..4  3..0
+-------+-----------+----------+------+------+-----+-------+
| rsvd  | attribute | card inst| slot | zone | p   | level |
+-------+-----------+----------+------+------+-----+-------+
    4        12          24        6      6     4       4
```

| Field | Width | Values |
| --- | --- | --- |
| Level | 4 | `kDuel` = 0, `kPlayer` = 1, `kZone` = 2, `kSlot` = 3, `kCard` = 4 |
| Player | 4 | 0..15 |
| Zone | 6 | 0..63 |
| Slot | 6 | 0..63 |
| Card instance | 24 | 0..16,777,215 |
| Attribute | 12 | 0..4,095; zero is `kSelf` |

`MakeAddress` packs all fields directly. The depth-specific constructors zero
fields they do not use. Accessors are `LevelOf`, `PlayerOf`, `ZoneOf`, `SlotOf`,
`CardOf`, and `AttributeOf`; `WithAttribute` replaces only the attribute field.

### Register word

**Tag:** `WordTag::kRegister` · **Payload:** 10 used bits, 50 reserved

```text
 59..10                         9..2   1..0
+------------------------------+------+----------+
| reserved                     | index| bank     |
+------------------------------+------+----------+
               50                  8        2
```

Bank 0 is `RegisterBank::kValue`, bank 1 is `kAddress`, and bank 2 is `kFlag`.
Bank 3 is invalid. Storage uses `kRegisterCount`, derived from the index width. `Reg(bank, index)` packs a selector; `RegisterBankOf` and
`RegisterIndexOf` extract its components.

### Immediate word

**Tag:** `WordTag::kImmediate` · **Payload:** 60 used bits

```text
 59     58..0
+------+-----------------------------------------------------------+
| sign | remaining two's-complement bits                            |
+------+-----------------------------------------------------------+
    1                              59
```

The range is **−2⁵⁹ through 2⁵⁹ − 1**, exposed as `kImmediateMin` and
`kImmediateMax`. `Imm` packs the value; `ImmediateValue` sign-extends bit 59.

```cpp
static_assert(ImmediateValue(Imm(-7)) == -7);
static_assert(ImmediateValue(Imm(kImmediateMin)) == kImmediateMin);
static_assert(Imm(0) != kNone);  // Same zero payload, different tags.
```

### Control word

**Tag:** `WordTag::kControl` · **Payload:** a 60-bit selector, currently 0..15

```text
 59..0
+------------------------------------------------------------+
| ControlReg selector                                        |
+------------------------------------------------------------+
                             60
```

| Selector | Value | State selected |
| --- | --- | --- |
| `ControlReg::kTurn` | 0 | Turn number |
| `ControlReg::kPhase` | 1 | Phase |
| `ControlReg::kStep` | 2 | Step |
| `ControlReg::kChain` | 3 | Chain |
| `ControlReg::kEffect` | 4 | Effect/frame |
| `ControlReg::kProgramCounter` | 5 | Executing instruction index |
| `ControlReg::kResultCount` | 6 | Actual movement count |
| `ControlReg::kResultSuccess` | 7 | Mutation success |
| `ControlReg::kEventKind` | 8 | Delivered event kind |
| `ControlReg::kEventSubject` | 9 | Delivered event subject |
| `ControlReg::kEventObject` | 10 | Delivered event object |
| `ControlReg::kEventContext` | 11 | Delivered event context |
| `ControlReg::kPendingDestination` | 12 | Staged destination operand |
| `ControlReg::kPendingSource0` | 13 | Staged first source operand |
| `ControlReg::kPendingSource1` | 14 | Staged second source operand |
| `ControlReg::kPendingCancelled` | 15 | Staged cancellation flag |

`Control(reg)` packs a selector and `ControlRegOf` extracts it. `kCount` is an
array-size sentinel, not a valid selector. A control operand identifies a cell;
its payload is not the current value of that cell.

### Operation word

**Tag:** `WordTag::kOperation` · **Payload:** 56 used bits, 4 reserved

```text
 59..56 55..40  39..32  31..20    19..8     7..0
+-------+-------+-------+----------+----------+--------+
| rsvd  | aux   | cause | flags    | subcode  | opcode |
+-------+-------+-------+----------+----------+--------+
    4      16       8       12         12        8
```

`Op(opcode, subcode, flags, cause, aux)` writes reserved bits as zero. The field
accessors are `OpcodeOf`, `SubcodeOf`, `FlagsOf`, `CauseOf`, and `AuxOf`.
`Sub(enum_value)` converts an enum for use in the subcode field.

For Summon, subcode bits 7..0 hold `SummonMethod` and bits 11..8 hold
`SummonMode`. `SummonSubcode`, `SummonMethodOf`, and `SummonModeOf` pack and unpack
that subdivision. `aux` holds small opcode-specific metadata.

### Instruction and program layout

An `Instruction` is `std::array<Word, 4>`. `MakeInstruction` defaults unused
operand slots to `kNone`:

```text
word index     0           1           2           3
          +------------+-----------+-----------+-----------+
          | operation  |    dst    |   src0    |   src1    |
          +------------+-----------+-----------+-----------+
width          64          64          64          64 bits
byte offset     0           8          16          24
```

Use `Operation`, `Dst`, `Src0`, and `Src1` to read the slots. The umbrella header
asserts an eight-byte `Word`, a 32-byte `Instruction`, and trivial copyability.
`EncodeBytes` writes each word least-significant byte first, in slot order.

For instruction index `pc`, the word offset is `pc * 4` and the byte offset is
`pc * 32`. Branch operands always contain the instruction index itself.

### Operand rules

These are the checks implemented by `ValidInstruction`. Every slot must also
pass `ValidOperand`, even if the opcode imposes no further rule on that slot.

| Opcode / builder | Required operands |
| --- | --- |
| `Nop`, `Halt` | All empty |
| `Set`, `Copy` | Writable dst; src0 present |
| `Load` | Register dst; src0 present |
| `Store` | Control or literal attribute dst; src0 present |
| `Swap` | dst and src0 present |
| `Select` | Address-register dst; literal address src0 |
| `Count` | Value-register dst; literal address src0 |
| `Move` | Whole-object literal addresses in dst and src0 |
| `Summon` | Whole-object slot/card dst; src0 present |
| `Position`, `Negate` | Address source dst |
| `Equip`, `ChangeControl` | Address sources in dst and src0 |
| `Counter` | Address source dst; src0 present |
| `Restrict` | Literal attribute dst; src0 present |
| `Alu`, `AluUnary` | Value-register dst; value sources; arity matches ALU subcode |
| `Compare` | Flag-register dst; both sources present |
| `Jump` | Empty dst/src1; immediate target src0 |
| `JumpIf` | Empty dst; flag-register src0; immediate target src1 |
| `Damage`, `GainLp`, `PayLp` | Address source dst; value source src0 |
| `RecordedRandom` | Value-register dst and immediate src0, or address-register dst and literal address src0 |
| `Event` | Nonzero event subcode |
| `Chain` | Subcode at most `ChainOp::kEnd` |

The categories used above correspond to predicates in
[Operand.hpp](include/lacooda/Operand.hpp):

| Category | Accepted kinds |
| --- | --- |
| Address source | Literal address or address register |
| Value source | Immediate, control, value/flag register, or literal attribute address |
| Writable | Any register, control selector, or literal attribute address |

### Encoding boundaries

Packing functions mask inputs to their field widths. They do not report overflow:
`V(256)` encodes `V(0)`, for example. Check IDs, indices, and immediate ranges
before packing when they originate outside the host's known schema.

Validation checks supported address levels and register banks, but accepts
nonzero reserved bits in address/register/operation words. It does not enforce
zero deeper fields on shallow addresses. Enum method subcodes are range-checked. Flag combinations and many
otherwise-unused operand slots remain unconstrained. New primitive operand shapes
and registration/branch target bounds are checked explicitly.

`IsNone` tests the tag; `ValidOperand` additionally requires a zero payload.
Other classification predicates likewise do not replace full validation.
Accessors decode fields and generally assume the caller has supplied the correct
word kind.

## Integrate an engine

Lacooda executes instructions and controls mutations. An embedding application
supplies storage through `runtime::StateAccess`, a seeded `std::mt19937`, and
player decisions. Storage exposes attributes, ordered members and relocation;
it does not receive effect names or source strings to interpret.

Existing card storage can remain in the embedding project. Addresses identify
instances rather than containing C++ pointers. If cards are kept in a vector,
its reallocations invalidate raw pointers: reserve adequate capacity or otherwise
provide stable storage before retaining read-only pointers. The adapter must
resolve identities after movement and reject stale references.

| Machine member | Representation |
| --- | --- |
| `regs.value` | `kRegisterCount` words; immediate, address or collection values as supported |
| `regs.address` | `kRegisterCount` words; runtime writes require valid addresses |
| `regs.flag` | `kRegisterCount` words; runtime writes require immediate zero or one |
| `control` | `ControlReg::kCount` words (currently 16) |

Value initialization leaves raw zero, the empty operand; it is not `Imm(0)`.
Initialize required inputs explicitly. The runtime checks register writes and
operand reads; editing public machine arrays bypasses those checks.

| Control index | Register | Contract |
| --- | --- | --- |
| 0–4 | TURN, PHASE, STEP, CHAIN, EFFECT | Caller/program context; no automatic duel rules |
| 5 | PC | Updated to the executing instruction index |
| 6 | RESULT_COUNT | Actual object count from movement |
| 7 | RESULT_SUCCESS | Tagged boolean mutation outcome |
| 8–11 | EVENT_KIND, EVENT_SUBJECT, EVENT_OBJECT, EVENT_CONTEXT | Delivered event context |
| 12–14 | PENDING_DESTINATION, PENDING_SOURCE0, PENDING_SOURCE1 | Staged instruction operands; require an active replacement participant |
| 15 | PENDING_CANCELLED | Immediate zero/one cancellation state |

Each frame has its own registers and PC. Runtime owns scheduling, modifiers,
event history and pending operations; referenced storage and RNG must outlive it.
There is no built-in synchronized multi-threaded access. Serialize access to a
single duel's runtime, storage, RNG and frames.

For reproducibility, use the same initial state, instruction semantics, ruleset,
seed, decisions and event ordering. A seed alone cannot reproduce different
external inputs. Record outcomes when needed, and version the surrounding
storage/protocol. The encoded stream has no version field: changing field widths
or reordering implicitly numbered enums can change its meaning.

## Source guide

Start with [Lacooda64.hpp](include/Lacooda64.hpp) for the public umbrella header. Each
module includes its own dependencies and can also be included directly.

| Layer | Headers | What to look for |
| --- | --- | --- |
| Representation | [Word](include/lacooda/Word.hpp), [Address](include/lacooda/Address.hpp), [Register](include/lacooda/Register.hpp), [Immediate](include/lacooda/Immediate.hpp), [Control](include/lacooda/Control.hpp) | Tags, layouts, constructors, and accessors |
| Instructions | [Opcode](include/lacooda/Opcode.hpp), [Operation](include/lacooda/Operation.hpp), [Instruction](include/lacooda/Instruction.hpp) | Opcode enums, operation fields, and the four-word array |
| Construction | [Builder](include/lacooda/Builder.hpp) | Instruction builder signatures |
| Checking | [Operand](include/lacooda/Operand.hpp), [Validate](include/lacooda/Validate.hpp) | Operand categories and exact validation rules |
| Storage | [Machine](include/lacooda/Machine.hpp) | Register arrays, encode/decode, and byte serialization |

Packing, extraction, builders, and single-instruction validation are generally
`constexpr` and `noexcept`. Program containers and serialization use dynamically
allocated vectors. The CMake project provides encoding checks through CTest. Enable them explicitly:

```sh
cmake -S . -B build -DLACOODA64_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The checks cover known encoded words and bytes, sequential decoding, field
boundaries, signed immediates, and trace validation/serialization.

## Repository layout

```text
Lacooda64/
├── CMakeLists.txt              # All libraries, tools and tests
├── README.md                   # Complete manual
├── LICENSE
├── include/
│   ├── Lacooda64.hpp           # Core umbrella
│   ├── lacooda/                # Public encoding and machine APIs
│   ├── runtime/                # Public execution APIs
│   └── assembly/               # Public assembly APIs
├── src/
│   ├── lacooda/                # Serialization and trace validation
│   ├── runtime/                # Interpreter and services; private Numeric.hpp
│   └── assembly/               # Parser, printer, lowering; private Spellings.def
├── tools/                      # Assembler CLI and Python source compiler
├── data/                       # Source effects, generated assembly and bindings
└── tests/                      # Core, runtime, assembly, effects and consumers
```

The core has no runtime or assembler dependency. Dataset names and source-language
rules stay in `tools/` and generated bindings. No generic effect opcode or
card-specific dispatch table is embedded in the core.

## Runtime execution

`Lacooda64::Runtime` executes numeric four-word instructions. Include
`runtime/Runtime.hpp`. There is no card-name catalogue, effect opcode or
string-valued action interpreter.

```cpp
using namespace openjoey::lacooda64;
using namespace openjoey::lacooda64::runtime;
std::mt19937 rng(seed);
Runtime runtime(storage, rng);  // storage implements StateAccess
Frame frame({Set(V(0), Imm(2)), Alu(V(1), V(0), Imm(3), AluOp::kAdd), Halt()});
auto result = runtime.Run(frame, 1000);
```

`StateAccess` exposes existing storage through Read, Write, Members and Relocate.
It does not execute effects. Property/zone IDs are supplied by the application;
no second card vector is required. State values are tagged words. Members must
return deterministic ordering; Relocate returns the new address. Storage must
resolve instance identities and reject stale accesses.

### Instructions and outcomes

All executable operands must satisfy the ISA validator. Runtime checks additionally
verify values, collection handles, storage access and numeric bounds. Unsupported
opcodes are reported explicitly; they are never treated as successful no-ops.

| Instruction | Runtime behavior and limits |
| --- | --- |
| NOP / HALT | Advance without mutation / terminate this frame |
| SET / COPY | Read an operand and write its tagged word; destination bank checks apply |
| LOAD | Dereference a literal/indirect attribute address, or read a register/control operand |
| STORE | Write an indirect attribute or writable operand; report mutation success |
| SWAP | Exchange two registers in the same bank; other destinations are unsupported |
| SELECT | Resolve and retain an address without reading its attribute value |
| COUNT | Count a storage container's members; invalid member addresses fault |
| ALU | Signed immediate arithmetic; division/modulo reject zero; results must fit the immediate range |
| COMPARE | Equality compares tagged words; ordering requires numeric immediates |
| JUMP | Set PC to the validated target |
| JUMPIF | ALWAYS, ZERO/FALSE and NOT_ZERO/TRUE are implemented; other conditions require COMPARE first |
| MOVE | Relocate up to the requested count (default one); ordinary movement clamps to availability; costs precheck quantity |
| DAMAGE / GAIN_LP / PAY_LP | Subtract/add/pay a nonnegative amount at an explicit numeric attribute; payment prechecks available value |
| RANDOM | Copy a recorded result without RNG use, or generate `[0,bound)` when the result operand is NONE |
| EVENT | Record/deliver the encoded event and queue matching subscriptions |
| SUMMON / POSITION / EQUIP / COUNTER / CONTROL / NEGATE / RESTRICT / CHAIN | Valid encodings exist; runtime returns `kUnsupported` |

The remaining primitives have these signatures:

| Instruction | Operands | Behavior |
| --- | --- | --- |
| ENUMERATE | Vdst, container or NONE | Snapshot a container; NONE creates an empty collection |
| AT | Adst, Vcollection, index | Checked collection lookup |
| APPEND | Vcollection, object | Append an object address |
| LENGTH | Vdst, Vcollection | Read collection size |
| ATTRIBUTE | Adst, object, field | Build an indirect attribute address |
| CHOOSE | Adst, Vcollection, player | Suspend for an object choice |
| CHOOSE | Vdst, count, player | Suspend for an integer in `[0,count)` |
| RANDOM | kind, Vdst, NONE, bound | Generate a seeded value in `[0,bound)` |
| SCHEDULE | Vhandle, PC, time | Capture a frame for a logical time |
| SUBSCRIBE | Vhandle, PC, event | Capture a frame for matching events |
| CANCEL | handle | Remove a schedule/subscription |
| MODIFY | Vhandle, attribute, value, method | Register ADD/SET/MULTIPLY/DIVIDE contribution |
| UNMODIFY | handle | Remove that contribution |
| HISTORY | Vdst, COUNT | Read event history size |
| HISTORY | dst, KIND/SUBJECT/OBJECT/CONTEXT, index | Read an event field |
| STAGE | Vhandle, instruction PC, event | Capture one mutation before applying it |
| COMMIT | handle | Apply amended operands once, or cancel |

Collection handles use `WordTag::kCollection`; they are invocation-local runtime
values, not literal operands in bytecode. Registers and collection snapshots are
copied when capturing a frame; program instructions are immutable shared storage.

Run returns `kHalted`, `kWaiting`, `kStepLimit`, `kInvalidProgram`, `kFault` or
`kUnsupported`. A step limit preserves progress for the next call. Overflow and
division by zero fail explicitly. A fault does not undo an already applied prefix.

For CHOOSE, inspect `frame.decision`, call `frame.Answer(index)`, then resume the
same frame. Invalid answers do not advance execution or consume randomness.
Candidates are a snapshot, not automatic live legality checks. Programs must
revalidate when required by their rules.

MOVE accepts indirect operands and reports actual movement through
`CONTROL.RESULT_COUNT` and `CONTROL.RESULT_SUCCESS`. Normal counts clamp to
availability; a cost with insufficient objects fails before moving any object.
A storage failure during a multi-object movement may leave a committed prefix.
Successful moves record and deliver `EventKind::kMoved`. MOVE's method describes
the movement; it does not automatically implement every game-rule consequence.
LP instructions currently require an explicit numeric attribute destination.

### Randomness and deferred execution

The caller supplies the duel's single `std::mt19937`. Bounded draws use a defined
64-bit rejection mapping, independent of standard-library distribution algorithms.
Equal initial state, seed, program and external decisions produce equal results.
Recorded outcomes consume no RNG. Add one for a die range starting at one.

`Advance(time)` queues due frames ordered by time then registration ID.
`Deliver(event)` records an event and queues matching subscription frames.
`TakeReady()` transfers the queue to the caller, which runs each frame with a
budget and handles any input suspension. Delivered frames expose
`CONTROL.EVENT_KIND`, `EVENT_SUBJECT`, `EVENT_OBJECT` and `EVENT_CONTEXT`.

Registrations capture their own handle, allowing a body to cancel itself.
Captured values do not follow later register changes. Timing phrases such as
"next controller Standby Phase" need compiled predicates or a bound numeric
event; they are not silently interpreted by the runtime.

### Modifiers and replacement handlers

Modifiers compose in registration order over the current stored base. Removing
one does not overwrite later base changes or other contributions. A card-property
modifier matches instance ID and attribute across location changes; storage must
resolve those identities consistently. A lifetime is explicit code that schedules
or subscribes a captured UNMODIFY operation. Automatic continuous-condition
reevaluation and a game's modifier-layer ordering still require rules programs.

STAGE captures one MOVE, attribute STORE, DAMAGE, GAIN_LP or PAY_LP instruction.
Handlers can use STORE to edit `CONTROL.PENDING_DESTINATION`, `PENDING_SOURCE0`,
`PENDING_SOURCE1` or `PENDING_CANCELLED` (immediate zero/one). COMMIT waits for all
handlers, validates the amended instruction and applies it at most once.
A failed handler prevents commit. Subscription order determines handler order.

`kWaiting` without a decision means replacement handlers are pending. Run the
frames from TakeReady before resuming the caller. They may themselves await input.
Staged control-register writes and arbitrary staged programs are rejected.

### Scope and remaining work

Tests cover scalar execution, choices, indirect movement, deterministic draws,
schedules, events, modifiers, history, replacement cancellation/redirection and
error cases. Effect-level tests cover a filtered target query and Mirage of
Nightmare's draw/deferred-discard body. They do not prove complete card rules.

Original SUMMON, POSITION, EQUIP, COUNTER, CONTROL, NEGATE, RESTRICT and CHAIN
instructions are encodable but their runtime semantics remain unimplemented and
return `kUnsupported`. SWAP currently supports one register bank; ordered JUMPIF
variants require COMPARE followed by a boolean branch.

The full corpus is converted; see [bindings and source gaps](#effect-corpus-and-bindings).
Activation/summon legality, battle/chain protocol,
visibility, continuous rules, replacement arbitration and transactional undo are
still outstanding. Suspended frames/registrations/collections/replacements do not
yet have a binary snapshot format. Copying a frame is not a full duel snapshot,
especially when pending replacements are shared.

## Assembly language

Link `Lacooda64::Assembly`; include `assembly/Assembler.hpp` or
`assembly/Disassembler.hpp`. `Assemble(text, symbols)` returns a trace and
line-numbered diagnostics. Errors clear the trace, never exposing a partial
executable result. Assembly has no runtime dependency.

```text
.address HAND [0:3:0]
.address DECK [0:2:0]
PC Instruction Meaning
0 COUNT V0, HAND                  ; Read current hand size.
1 ALU V0, #4, V0, SUBTRACT        ; Calculate missing cards.
2 ALU V0, V0, #0, MAXIMUM         ; Clamp to zero.
3 MOVE DRAW, HAND, DECK, V0       ; Draw the available amount.
4 HALT
```

The zone IDs above are example bindings, not library conventions. `.address`
defines addresses; `.symbol FIELD_ATK #1` defines another operand alias. Callers
can instead supply Symbols. Numeric input is checked against configured widths.
Registers are Vn/An/Fn, immediates use `#`, and unused operands use `NONE`.
Address forms are `[a]`, `[p:a]`, `[p:z:a]`, `[p:z:sl:a]`, `[p:z:sl:c:a]`.

PCs are optional; explicit PCs must be contiguous. Labels end in `:` and occupy
no instruction. Branch/deferred targets resolve in a second pass. `;` begins a
comment. The `PC Instruction Meaning` header is optional.

| Family | Operands |
| --- | --- |
| SET/COPY/LOAD/STORE/SWAP/SELECT/COUNT/DAMAGE/GAIN_LP/PAY_LP | dst, source |
| COMPARE | flag, lhs, rhs, comparison |
| ALU | dst, lhs, rhs, method; unary: dst, source, method |
| JUMP | PC or label |
| JUMPIF | flag, PC or label, optional condition (TRUE by default) |
| MOVE | method, destination, source, optional count |
| SUMMON | method, mode, destination, source |
| POSITION/NEGATE | method, target |
| EQUIP/COUNTER/CONTROL/RESTRICT | method, dst, source |
| RANDOM | kind, dst, recorded result or NONE, optional bound |
| EVENT | kind, optional subject, object, context |
| CHAIN | method, optional subject, context |
| NOP/HALT | none |

See [runtime contracts](#runtime-execution) for new primitive syntax.
Encodable instructions are not necessarily implemented in the runtime.

Operation metadata uses a checked suffix, for example
`MOVE SEND, A0, A1, #1 | flags=4 cause=3`. Optional `aux=N` is also supported.
Unknown, repeated or out-of-range metadata is rejected.

`AssembleModule(text, symbols)` accepts `.program NAME` / `.end` blocks, returning
named traces with independent PCs and label scopes. Plain single-program text
is accepted as an unnamed program. Module errors clear all returned programs.
Diagnostics refer to lines in the original module.

Disassemble emits readable numbered instructions, including operation metadata.
Noncanonical bits that readable syntax cannot preserve fall back to four exact
`WORDS 0x...` operands. Round trips preserve operation flags, causes and otherwise-unused bits.

### Numeric lowering helpers

`Lowering.hpp` builds instruction sequences, not interpreted action records:

- Filter: enumeration, indexing, property reads, comparisons, branches and append.
- DrawToSize: count, arithmetic, movement and actual-result capture.
- RandomMove: repeated seeded selection/movement without repeating removed cards.
- OnceOnEvent: subscribe, skip the body, cancel the subscription before execution.

Scratch registers are allocated with width-based bounds. Allocate operands from
the same builder when mixing these helpers with manually emitted instructions.

The separate Python source compiler in `tools/` converts all 120 cards and 264
effect blocks to `data/effects.lasm`. The assembly layer accepts the resulting
native programs without knowing the effect-source language. See the
[corpus guide](#effect-corpus-and-bindings) for bindings, regeneration and source gaps.

### CLI

```sh
cmake -S . -B build -DLACOODA64_BUILD_TESTS=ON -DLACOODA64_BUILD_TOOLS=ON
cmake --build build
build/lacooda-asm input.lasm output.lasm
ctest --test-dir build --output-on-failure
```

The optional output is canonical text. Existing Encode/EncodeBytes provide word
and little-endian byte serialization of the resulting trace.

## Effect corpus and bindings

`effects.lasm` contains **all 264 effect blocks from the 120 cards in
`effects.txt`**, converted to numbered native Lacooda instructions. Each block
has its own `.program CARD_ID_EFFECT_NUMBER` / `.end` boundary, registers and
program counter. Comments identify the original source line and explain each
instruction. `effects.txt` is unchanged.

The conversion compiles the structured `EFFECT` blocks. The cards' prose `TEXT`
paragraphs are reference material, not a second input language. Differences
between those two representations are not automatically repaired.

### Generate and check

From the repository root:

```sh
python3 tools/convert_effects.py
python3 tools/convert_effects.py --check
cmake -S . -B build -DLACOODA64_BUILD_TESTS=ON -DLACOODA64_BUILD_TOOLS=ON
cmake --build build
build/lacooda-asm data/effects.lasm
ctest --test-dir build --output-on-failure
```

The converter uses Python 3.10+ and the standard library. `--check` regenerates
both outputs in memory and fails if either committed file differs. Conversion
errors include the card ID, effect number and source line. No output is written
until every block has compiled.

| File | Purpose |
| --- | --- |
| `effects.txt` | Original structured effect dataset |
| `effects.lasm` | Native instruction programs, suitable for `AssembleModule` |
| `effects.bindings.json` | Numeric state/event bindings, invocation inputs, source coverage and diagnostics |
| `tools/effect_source.py` | Source parser, including nested choices and conditionals |
| `tools/effect_compile.py` | Source-to-instruction lowering |
| `tools/convert_effects.py` | Reproducible conversion entry point |

There are no `.constant` action records or generic effect opcodes. Queries become
`ENUMERATE`, `AT`, `ATTRIBUTE`, `LOAD`, comparisons and collection-building loops.
Decisions become `CHOOSE`; random selections use `RANDOM` with the duel's seed.
Arithmetic, state changes, delayed bodies and modifier removal are instructions,
not strings handed to an effect callback.

### Binding the programs

The JSON manifest is the contract between these generated programs and duel
storage. Its numeric IDs are allocated deterministically for this source order;
regenerate and deploy assembly and bindings together. They are **not** additions
to the core ISA. A source edit can change their allocation.

| Manifest section | Meaning |
| --- | --- |
| `source_sha256` | Digest of the source used for this conversion |
| `fields` | Attribute IDs for card, player, duel and event-record properties |
| `enums` | Numeric values grouped by domain, such as phase, position and card name |
| `zones` | Zone IDs used in literal container addresses |
| `events` | Source event IDs used by guards and subscriptions |
| `programs` | Card/effect identity, source kind/optionality, instruction count, inputs and warnings |

Each program's `inputs` lists the registers to initialize. Common inputs are:

- `A0`: the source card's object address, with attribute zero.
- `A1`: the current event record's object address when event properties are read.
- `V0`: controller, as tagged immediate zero or one.
- `V1`: opponent, as tagged immediate one or zero.
- `CONTROL.EVENT_KIND`: the source event ID for event-guarded entry points.

Additional named inputs appear only where the source uses an undefined reference
or omits a candidate set. Collection inputs are handles created in that frame's
`Collections`, not integers copied from another frame. An optional effect's entry
is invoked after activation is accepted; its `optional` metadata does not add an
extra decision at every instruction.

State values are tagged words. Numeric properties, booleans, enum values,
counters and handles use immediates; object references use address words.
`IS.*`, `TAG.*`, `CAN.*`, aliases, restrictions and internal modifier slots have
zero as their absent value. Do not default every missing numeric property to
zero: missing required stats, identities or event fields indicate incomplete
state. Nullable references can compare against immediate zero, but must contain
a valid address before an instruction dereferences them.

`Members` must expose deterministic, top-first zone snapshots. Player containers
expose their cards; the duel container exposes all card instances, including
cards that changed zones. Event `TARGETS` and `RANDOM_RESULTS` are object addresses
whose members enumerate the corresponding objects. Identity resolution must
preserve captured references across movement. `ORDER_INDEX` writes must update
the ordering seen by later snapshots.

Programs write numeric rule properties such as restrictions and positions.
Reading and enforcing those properties in summon, combat and activation rules
is a separate requirement for a complete duel. Storage does not interpret the
source notation or dispatch effect names.

### Deferred execution and lifetimes

A deferred body is part of its program. `SUBSCRIBE` captures the current registers
and collections, while normal execution jumps past the body. Phase bodies read
the current phase and turn player. A matching one-shot body cancels its own
subscription. Second/third-phase delays use successive registrations, so they
do not depend on mutating an already captured counter.

Source event IDs and native `EventKind` IDs are separate namespaces. Delivery of
source events uses the manifest's IDs. When a subscribed trigger reads event
fields, `CONTROL.EVENT_CONTEXT` must contain its event-record address. Recording
a native move event alone does not synthesize every source event or history flag.

Numeric modifiers have explicit removal instructions. `THIS_EFFECT` modifiers
are removed on the program's normal exit; turn/phase modifiers subscribe for
expiry. Continuous programs clear their previous contributions before reevaluating
guards. The duel rules must invoke those programs on relevant state changes.
The current continuous cleanup uses card-definition/effect-specific slots;
independent simultaneous instances of the same definition need instance-specific
contribution storage before this mechanism is suitable for a full duel.

### Source gaps and execution coverage

Warnings are attached to the exact program and source line in the manifest:

- Toon Summoned Skull effect 5 and Insect Queen effect 2 contain explicit `NOP`
  resolutions in the source. They remain `NOP` instructions.
- Vampire Genesis, Armed Dragon LV5, Buster Rancher and Ectoplasmer read a
  reference before selecting it. That reference is an explicit invocation input;
  the later source selection is preserved. Resolving the ordering requires a
  source correction, not an invisible compiler rewrite.
- Pandemonium and Rope of Life provide a selection count without a candidate set.
  Their candidate collections are explicit inputs.
- Reasoning omits the exhausted-deck/no-match branch. The generated branch sends
  the excavated cards to the GY and stops; it never summons the last nonmatching
  card. This additional branch is recorded as a warning.

The structured source also omits some prose timing, once-per-turn and legality
conditions, and contains repeated maintenance payments. Conversion preserves
its stated instructions; it does not certify the dataset against card rulings.

Every program is checked by the assembler and round-tripped through text and
word encoding. A runtime regression executes the **generated Mirage of Nightmare
program**, including wrong-player phase rejection, the captured draw count,
seeded discard without replacement and one-shot cancellation.

This is complete **source-block conversion**, not complete duel execution.
`SUMMON` is encoded but still unsupported by the current runtime. Rule-property
enforcement, source-event production, complete continuous-rule handling and
activation/battle/chain rules remain engine work. See the
[runtime coverage](#runtime-execution) before executing other programs.

## Command-line tools

`convert_effects.py` compiles all structured blocks in `data/effects.txt` into
`data/effects.lasm` and `data/effects.bindings.json`. Run it from any directory:

```sh
python3 tools/convert_effects.py
python3 tools/convert_effects.py --check
```

The converter requires Python 3.10+ and no third-party packages. `effect_source.py`
parses the dataset; `effect_compile.py` lowers it to native instructions.
Unknown operations and unsupported constructs fail with source locations.
Generation finishes in memory before updating output files. `--check` changes
nothing and fails when either output is stale.

`lacooda-asm.cpp` builds as `lacooda-asm` with `LACOODA64_BUILD_TOOLS=ON`:

```sh
build/lacooda-asm data/effects.lasm
build/lacooda-asm data/effects.lasm /tmp/canonical-effects.lasm
```

It accepts single programs and `.program` / `.end` modules, validates all operand
and branch encodings, and optionally writes a lossless text listing. PCs restart
at zero for each program. A failed module never exposes a partial executable
result through the assembly API.

Dataset-specific source handling stays here. The ISA, runtime and assembler do
not know card names or interpret source action records. See the
[corpus guide](#effect-corpus-and-bindings) for the binding contract and source diagnostics.

## Executable runtime example

This complete program assembles an LP change, executes it through Lacooda and
checks the resulting storage. Its adapter intentionally exposes only one numeric
field. A real adapter also implements membership and relocation for its cards.
Link it with both `Lacooda64::Runtime` and `Lacooda64::Assembly`.

```cpp
#include <iostream>
#include <random>
#include "assembly/Assembler.hpp"
#include "runtime/Runtime.hpp"

using namespace openjoey::lacooda64;

class DuelStorage final : public runtime::StateAccess {
 public:
  Word lp = Imm(8000);
  bool Read(Address field, Word& value) const override {
    if (field != Player(0, 1)) return false;
    value = lp;
    return true;
  }
  bool Write(Address field, Word value) override {
    if (field != Player(0, 1) || !IsImmediate(value)) return false;
    lp = value;
    return true;
  }
  bool Members(Address, std::vector<Address>&) const override { return false; }
  bool Relocate(Address, Address, Address&) override { return false; }
};

int main() {
  const auto assembled = assembly::Assemble(
      ".address P0.LP [0:1]\n"
      "0 DAMAGE P0.LP, #500\n"
      "1 HALT\n");
  if (!assembled.ok()) return 1;
  DuelStorage storage;
  std::mt19937 rng(42);
  runtime::Runtime engine(storage, rng);
  runtime::Frame frame(assembled.trace);
  const auto result = engine.Run(frame, 100);
  if (result.status != runtime::Status::kHalted) return 2;
  std::cout << ImmediateValue(storage.lp) << '\n';  // 7500
  return ImmediateValue(storage.lp) == 7500 ? 0 : 3;
}
```

## Opcode and subcode spelling reference

The following spellings are accepted by the assembler. Enum declaration order and
explicit numeric values are defined in [Opcode.hpp](include/lacooda/Opcode.hpp);
`Sub(enum_value)` obtains the encoded subcode. SUMMON combines method and mode
using `SummonSubcode`. Unknown spellings are errors. Flags, causes and aux values
use the numeric metadata suffix described above.

| Enum domain | Assembly spellings |
| --- | --- |
| `Opcode` | `NOP`, `SET`, `LOAD`, `STORE`, `COPY`, `SWAP`, `SELECT`, `COUNT`, `MOVE`, `SUMMON`, `POSITION`, `EQUIP`, `COUNTER`, `CONTROL`, `NEGATE`, `RESTRICT`, `ALU`, `COMPARE`, `JUMP`, `JUMPIF`, `DAMAGE`, `GAIN_LP`, `PAY_LP`, `RANDOM`, `EVENT`, `CHAIN`, `HALT`, `ENUMERATE`, `AT`, `APPEND`, `LENGTH`, `ATTRIBUTE`, `CHOOSE`, `SCHEDULE`, `SUBSCRIBE`, `CANCEL`, `MODIFY`, `HISTORY`, `STAGE`, `COMMIT`, `UNMODIFY` |
| `HistoryField` | `COUNT`, `KIND`, `SUBJECT`, `OBJECT`, `CONTEXT` |
| `ModifierOp` | `ADD`, `SET`, `MULTIPLY`, `DIVIDE` |
| `MoveMethod` | `RELOCATE`, `DRAW`, `SEARCH`, `MILL`, `DISCARD`, `TRIBUTE`, `DESTROY`, `SEND`, `BANISH`, `RETURN`, `ATTACH_MATERIAL`, `DETACH_MATERIAL` |
| `SummonMethod` | `NORMAL`, `TRIBUTE`, `FLIP`, `SPECIAL`, `FUSION`, `RITUAL`, `TOKEN` |
| `SummonMode` | `DEFAULT`, `FACE_UP_ATTACK`, `FACE_UP_DEFENSE`, `FACE_DOWN_DEFENSE`, `SET` |
| `AluOp` | `ADD`, `SUBTRACT`, `MULTIPLY`, `DIVIDE`, `MODULO`, `MINIMUM`, `MAXIMUM`, `NEGATE`, `ABSOLUTE` |
| `CompareOp` | `EQUAL`, `NOT_EQUAL`, `LESS`, `LESS_EQUAL`, `GREATER`, `GREATER_EQUAL` |
| `PositionOp` | `ATTACK`, `DEFENSE`, `FACE_UP`, `FACE_DOWN`, `FACE_UP_ATTACK`, `FACE_UP_DEFENSE`, `FACE_DOWN_DEFENSE`, `TOGGLE` |
| `NegateOp` | `ACTIVATION`, `EFFECT`, `SUMMON`, `ATTACK` |
| `EquipOp` | `ATTACH`, `DETACH`, `TRANSFER` |
| `CounterOp` | `PLACE`, `REMOVE`, `TRANSFER`, `SET` |
| `ControlOp` | `TAKE`, `GIVE`, `SWAP`, `RETURN` |
| `RestrictOp` | `APPLY`, `CLEAR`, `INCREMENT`, `DECREMENT` |
| `RandomKind` | `DIE`, `COIN`, `CHOICE`, `RANDOM_CARD`, `SHUFFLE_SWAP`, `CUT_POINT` |
| `ChainOp` | `BEGIN`, `PUSH`, `PASS`, `BEGIN_RESOLVE`, `RESOLVE_LINK`, `POP`, `END_RESOLVE`, `END` |
| `JumpCondition` | `ALWAYS`, `ZERO`, `NOT_ZERO`, `TRUE`, `FALSE`, `EQUAL`, `NOT_EQUAL`, `LESS`, `LESS_EQUAL`, `GREATER`, `GREATER_EQUAL` |
| `EventKind` | `NONE`, `TURN_BEGIN`, `TURN_END`, `PHASE_BEGIN`, `PHASE_END`, `STEP_BEGIN`, `STEP_END`, `TRIGGER`, `ACTIVATE`, `RESOLVE_BEGIN`, `RESOLVE_END`, `ACTIVATION_NEGATED`, `EFFECT_NEGATED`, `DECLARE`, `SELECT`, `TARGET`, `REVEAL`, `INSPECT`, `EXCAVATE`, `DIE_ROLLED`, `COIN_TOSSED`, `SHUFFLE`, `CUT`, `WOULD_MOVE`, `MOVED`, `REPLACEMENT`, `WOULD_SUMMON`, `SUMMONED`, `POSITION_CHANGED`, `CONTROL_CHANGED`, `EQUIPPED`, `UNEQUIPPED`, `COUNTER_CHANGED`, `ATTACK_DECLARED`, `ATTACK_TARGETED`, `ATTACK_DIRECT`, `ATTACK_CANCELED`, `BATTLE_REPLAY`, `BATTLE`, `BATTLE_DAMAGE`, `DECK_OUT`, `WIN`, `DRAW` |
| `CauseKind` | `UNSPECIFIED`, `RULE`, `PLAYER_ACTION`, `CARD_EFFECT`, `BATTLE`, `SUMMON_PROCEDURE`, `MAINTENANCE`, `REPLACEMENT` |

## Public API contracts

The core umbrella exports only the core. Include runtime and assembly headers
explicitly. Constructors and methods that allocate containers may propagate
standard allocation exceptions; runtime status values report instruction failures,
not a blanket exception boundary. Encoding helpers are intentionally unchecked
packers: validate untrusted values before calling them if truncation is unwanted.

| API | Inputs and result | Failure and lifetime contract |
| --- | --- | --- |
| `Encode(trace)` | Four words per instruction | Does not validate; preserves all bits |
| `Decode(words)` | `DecodeResult` with trace, error and word offset | Misalignment or invalid instruction; may retain a valid prefix; does not check jump bounds |
| `EncodeBytes(trace)` | Little-endian byte vector | 32 bytes/instruction; no envelope/version/checksum; no byte decoder is currently exposed |
| `WriteU64Le(out, word)` | Appends eight bytes | Does not clear the existing buffer |
| `ValidateTrace(trace)` | Error and offending instruction index | Checks instruction shapes and jump/registration/staging targets; not game legality |
| `Frame(trace)` | Owns immutable program through shared storage | Copies/moves instruction storage; initializes registers to empty words |
| `Frame::Answer(index)` | Boolean acceptance | Requires a pending decision, matching PC, valid index and no prior answer |
| `Runtime::Run(frame, budget)` | Status, PC, consumed steps and message | Budget is per call; preserve the frame to resume; faults do not roll back a committed prefix |
| `Runtime::Deliver(event)` | Records event and queues subscribed frames | Does not run handlers synchronously |
| `Runtime::Advance(time)` | Queues due logical-time registrations | No wall-clock sleep; caller controls time progression |
| `Runtime::TakeReady()` | Transfers ready frames | Caller handles budgets, decisions and completion |
| `Collections::Add(objects)` | Tagged collection handle | Handle is local to these collections; copied frame snapshots preserve their handles |
| `Collections::Get(handle)` | Mutable vector pointer or null | Rejects wrong tags/out-of-range handles; adding collections may invalidate returned pointers |
| `Modifiers::Add(field, operation, value)` | Numeric handle | Contribution order is registration order; ID exhaustion throws |
| `Modifiers::Remove(handle)` | Whether an entry existed | Repeated removal returns false |
| `Modifiers::Read(state, field, value)` | Base read plus matching contributions | Fails on unavailable/non-numeric data or arithmetic failure |
| `Scheduler::Register(kind, key, frame, result)` | Captured registration handle | Copies frame state; writes handle to a value result register when requested |
| `Scheduler::Cancel(handle)` | Whether an entry existed | Does not remove a handler already transferred to a caller |
| `Scheduler::Due(time)` | Due frames ordered by time and registration ID | Consumes due time registrations |
| `Scheduler::Event(kind)` | Copies of matching captured frames | Persistent subscriptions remain until cancelled |
| `Replacements::Stage(instruction)` | Pending operation handle | ID exhaustion throws; Runtime validates supported staging semantics |
| `Replacements::Find(handle)` / `Erase(handle)` | Shared pending operation / removal | Missing lookup returns null; existing shared references can outlive registry removal |
| `PendingParticipant::Complete(success)` | Completes participant once | Decrements outstanding count and remembers failure |
| `Assemble(text, symbols)` | Trace and line-numbered diagnostics | Any error clears the trace; labels are local to the input |
| `AssembleModule(text, symbols)` | Named programs and diagnostics | Any error clears every program; PCs and symbols are scoped per program |
| `Disassemble(trace)` | Lossless numbered text | Reassembles internally; raw words preserve otherwise unrepresentable fields |
| `Lowering::Value/AddressRegister/Flag()` | Scratch operand | Finite register bank; exhaustion throws |
| `Lowering::Emit/Here/PatchJump()` | Instruction index / current PC / patch | Targets are checked when publishing the trace |
| `Lowering::Filter/DrawToSize/RandomMove/OnceOnEvent()` | Native sequences and result operands | No source-language callbacks in runtime; build-time callbacks emit code |
| `Lowering::Finish()` | Trace ending in HALT | Rejects invalid instruction forms/targets |

`StateAccess` implementations return `false` on unsupported or stale accesses.
`Read` and `Write` operate on attribute addresses. `Members` returns object
addresses in deterministic order. `Relocate` must return the new location while
preserving instance identity. Failed storage operations are not automatically
transactional, and cost prechecks do not guarantee rollback after a later storage
failure.

`ModifiedState` forwards writes, membership and relocation to underlying storage
and applies modifiers on reads. `ResolveAddress`, `ReadOperand` and `WriteOperand`
are low-level runtime helpers for the same addressing and register rules used by
the interpreter. `Decision` contains PC, player, candidate count and optional
object candidates; answers are candidate indices, never raw card IDs. `Event`
contains kind, subject, object and context words. `Result` contains status, PC,
steps and a diagnostic message. Assembly `Diagnostic::line` is one-based.

## Validation, testing and troubleshooting

The root CMake file defines the complete test suite. Encoding checks preserve
known words/bytes and compile-time assertions. Runtime checks cover arithmetic,
choices, movement, randomness, modifiers, schedules, history, costs, faults and
replacement handlers. Assembly checks cover syntax, malformed input and exact
round trips. Corpus checks assemble all 264 programs, serialize them and execute
a representative generated effect. Python checks verify source coverage,
diagnostics and regeneration freshness. The consumer test links multiple C++
translation units against the public targets.

For an instrumented build with GCC or Clang:

```sh
cmake -S . -B build-sanitize -DLACOODA64_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Wshadow -Wconversion -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

| Symptom | Check |
| --- | --- |
| Undefined references after upgrading | Link Core/Runtime/Assembly; ordinary definitions now live in compiled libraries |
| Public header not found | Use the CMake target or `-I/path/to/Lacooda64/include` |
| `kUnsupported` | The instruction is encodable but runtime semantics or that variant are not implemented |
| `kInvalidProgram` | Validate instruction forms and all PC targets before execution |
| `kFault` | Inspect PC/message, initialized tagged inputs, storage availability and numeric range |
| `kWaiting` with a decision | Answer an index and resume the same frame |
| `kWaiting` without a decision | Run queued replacement participants before retrying COMMIT |
| `kStepLimit` | Resume the same frame with another budget; inspect loops if progress is unexpected |
| Different seeded results | Compare initial state, candidate order, decisions, event ordering and RNG consumption |
| Conversion freshness failure | Regenerate assembly and bindings together with `tools/convert_effects.py` |
| Truncated address/register values | Packers mask by design; reject oversized user inputs before encoding |

Changing layout constants or implicitly numbered enums changes binary meaning.
This restructuring preserves their values. Strict `-Wshadow` builds may still
report existing `kNone` enum-name shadowing; the extraction does not rename the
public enum API. `InstructionFlag` remains an unscoped bitmask enum.

## License

See [LICENSE](LICENSE) for the GNU General Public License, version 3.
