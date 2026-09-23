# Lacooda64

**A 64-bit instruction format for card-game duels.**

Lacooda64 gives OpenJoey2 a common representation for duel operations: moving a
card, changing life points, comparing values, branching, and recording events.
Addresses and operands fit into tagged 64-bit words. Every instruction occupies
four words—32 bytes—so a program is a sequence with fixed boundaries and explicit
operand types.

The library is **header-only C++20**, with no dependencies beyond the standard
library. It supplies the instruction set, builders, validators, register storage,
and serialization. The surrounding game engine interprets those instructions
and applies the rules of the duel.

```cpp
using namespace openjoey::lacooda64;

constexpr auto damage = Damage(Player(1), Imm(500));
constexpr auto draw = Move(Zone(0, 3), Zone(0, 2), MoveMethod::kDraw);
constexpr auto check = Compare(F(0), V(0), Imm(3), CompareOp::kGreaterEqual);

static_assert(ValidInstruction(damage));
```

These calls **construct instructions**. Executing them is the host engine's job.
Zone IDs in this README are examples chosen by the host ruleset.

[Get started](#get-started) · [Build a duel effect](#build-a-duel-effect) ·
[Work with operands](#work-with-operands) · [Instruction set](#instruction-set) ·
[Validate and serialize](#validate-and-serialize) · [Binary format](#binary-format) ·
[Integrate an engine](#integrate-an-engine) · [Source guide](#source-guide)

## Get started

Place the repository at `external/Lacooda64` in your project:

```text
my-engine/
├── CMakeLists.txt
├── main.cpp
└── external/
    └── Lacooda64/
        ├── CMakeLists.txt
        ├── Lacooda64.hpp
        └── lacooda/
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

You can also compile directly, using the repository root as an include directory:

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Iexternal/Lacooda64 main.cpp -o duel
./duel
```

`Lacooda64::Lacooda64` is a CMake `INTERFACE` target. Configuring and building this
repository by itself produces no binary; compilation happens in the consuming
application. There are currently no installation or `find_package` rules.

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

[Example.hpp](lacooda/Example.hpp) provides these instruction building blocks as
`example::kCountMonsters`, `kCompareThree`, `kJumpToEffect`, `kDrawTwo`, and `kStop`.

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
`src1`; the host writes the result into `dst` when interpreting it. Event operands
represent subject, object, and context. Their meaning is agreed with the engine.

## Instruction set

The builders in [Builder.hpp](lacooda/Builder.hpp) cover every opcode family.
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
| Outcomes | `RecordedRandom` | Record a resolved random or choice result |
| Protocol | `Event`, `Chain` | Record events and chain-resolution steps |

### Variants and metadata

An operation carries an opcode, subcode, flags, cause, and a compact auxiliary
value. Subcodes select variants such as `MoveMethod::kDraw`, `AluOp::kAdd`, or
`CompareOp::kGreaterEqual`. [Opcode.hpp](lacooda/Opcode.hpp) defines the enums.

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

64-bit words → Decode → ValidateTrace → host interpreter
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
`Decode` allocates inside a `noexcept` function; allocation failure terminates.

The library currently has no byte-buffer decoder. A host reading bytes must check
that the length is a multiple of 32, reconstruct words from groups of eight
little-endian bytes, and then call `Decode` and `ValidateTrace`. A file or network
protocol must supply its own framing, version, and ruleset identification.

## Binary format

Bit ranges below are inclusive. Bit 0 is the least-significant bit. Each encoded
word has a four-bit tag in bits 63..60 and a 60-bit payload in bits 59..0.

```text
 63..60 59..0
+------+------------------------------------------------------------+
| tag  | payload                                                    |
+------+------------------------------------------------------------+
    4                              60
```

`Word` is `std::uint64_t`; `SignedWord` is `std::int64_t`. `Address`, `Operand`,
`OperationWord`, and the identifier types are aliases of `Word`. Tags distinguish
encoded values at runtime; the aliases do not introduce separate C++ types.

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
| `WordTag::kReserved7` | `0x7` | Reserved |

`MakeTagged`, `TagOf`, `PayloadOf`, and `IsTag` construct and inspect this outer
layout. Operation words belong in instruction slot 0. Label and reserved tags
are not currently valid operands.

### Address word

**Tag:** `WordTag::kAddress` · **Payload:** 53 used bits, 7 reserved

```text
 59..53 52..50 49..48 47..42 41..36 35..12     11..0
+-------+-------+------+-------+-------+-----------+-----------+
| rsvd  | level | p    | zone  | slot  | card inst | attribute |
+-------+-------+------+-------+-------+-----------+-----------+
    7       3      2       6       6        24          12
```

| Field | Width | Values |
| --- | --- | --- |
| Level | 3 | `kDuel` = 0, `kPlayer` = 1, `kZone` = 2, `kSlot` = 3, `kCard` = 4 |
| Player | 2 | 0..3 |
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
 59..10                         9..8   7..0
+------------------------------+------+----------+
| reserved                     | bank | index    |
+------------------------------+------+----------+
               50                  2        8
```

Bank 0 is `RegisterBank::kValue`, bank 1 is `kAddress`, and bank 2 is `kFlag`.
Bank 3 is invalid. `Reg(bank, index)` packs a selector; `RegisterBankOf` and
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

**Tag:** `WordTag::kControl` · **Payload:** a 60-bit selector, currently 0..5

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
| `ControlReg::kProgramCounter` | 5 | Next instruction index |

`Control(reg)` packs a selector and `ControlRegOf` extracts it. `kCount` is an
array-size sentinel, not a valid selector. A control operand identifies a cell;
its payload is not the current value of that cell.

### Operation word

**Tag:** `WordTag::kOperation` · **Payload:** 56 used bits, 4 reserved

```text
 59..52  51..40    39..28    27..20  19..4       3..0
+--------+----------+----------+--------+------------+------+
| opcode | subcode  | flags    | cause  | aux        | rsvd |
+--------+----------+----------+--------+------------+------+
     8        12         12        8         16         4
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
[Operand.hpp](lacooda/Operand.hpp):

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
zero deeper fields on shallow addresses. Most subcode ranges, flag combinations,
and otherwise-unused operand slots are also unconstrained. ALU arity/subcodes
and Chain subcodes have explicit checks; Event checks only for a nonzero subcode.

`IsNone` tests the tag; `ValidOperand` additionally requires a zero payload.
Other classification predicates likewise do not replace full validation.
Accessors decode fields and generally assume the caller has supplied the correct
word kind.

## Integrate an engine

The host connects these encoded operations to real game state. A typical
execution cycle fetches the instruction at the PC, dispatches on its opcode,
resolves its operands, applies the effect, and advances or replaces the PC.

`MachineState` provides the VM's register storage:

| Member | Storage |
| --- | --- |
| `regs.value` | 256 `Word` cells |
| `regs.address` | 256 `Word` cells |
| `regs.flag` | 256 `Word` cells |
| `control` | Six `Word` cells indexed with `ControlIndex` |

All cells initially contain raw zero when value-initialized. The host chooses
whether and where tagged values are required; it must initialize `Imm(0)`
explicitly when that is the intended cell value. The arrays do not enforce
register-bank content conventions.

The engine also owns the duel objects, zone and attribute definitions, card
instance lifetimes, arithmetic behavior, chain protocol, and event handling.
It decides how control-register values are represented, how PC changes are
applied, and how execution is bounded. None of these rules is inferred from a
word's tag alone.

For replay, both producer and consumer need the same instruction semantics and
ruleset. Record resolved outcomes where necessary and version the surrounding
storage/protocol. The encoded stream has no version field: changing field widths
or reordering implicitly numbered enums can change its meaning.

## Source guide

Start with [Lacooda64.hpp](Lacooda64.hpp) for the public umbrella header. Each
module includes its own dependencies and can also be included directly.

| Layer | Headers | What to look for |
| --- | --- | --- |
| Representation | [Word](lacooda/Word.hpp), [Address](lacooda/Address.hpp), [Register](lacooda/Register.hpp), [Immediate](lacooda/Immediate.hpp), [Control](lacooda/Control.hpp) | Tags, layouts, constructors, and accessors |
| Instructions | [Opcode](lacooda/Opcode.hpp), [Operation](lacooda/Operation.hpp), [Instruction](lacooda/Instruction.hpp) | Opcode enums, operation fields, and the four-word array |
| Construction | [Builder](lacooda/Builder.hpp), [Example](lacooda/Example.hpp) | Builder signatures and worked instruction constants |
| Checking | [Operand](lacooda/Operand.hpp), [Validate](lacooda/Validate.hpp) | Operand categories and exact validation rules |
| Storage | [Machine](lacooda/Machine.hpp) | Register arrays, encode/decode, and byte serialization |

Packing, extraction, builders, and single-instruction validation are generally
`constexpr` and `noexcept`. Program containers and serialization use dynamically
allocated vectors. The CMake project currently defines no CTest suite.

## License

See [LICENSE](LICENSE) for the GNU General Public License, version 3.
