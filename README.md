# OpenJoey2 — Lacooda 64-bit Word VM

`Lacooda64` is a **header-only C++20 library** that defines a compact,
self-describing **bytecode virtual machine** used to describe and replay the
state changes of a card-game duel (a Yu‑Gi‑Oh!-style rules model: players,
zones, hands, decks, life points, attacks, chains, counters, etc.).

Everything in the *executable duel* layer is represented as a stream of
64‑bit words. Addresses, operands, registers, control values, operation words
and instructions are all built from the same tagged `Word` type, so a single,
uniform encoding can be stored on disk, sent over a wire, or replayed in order
to reconstruct game state.

## Table of Contents

- [Quick start](#quick-start)
- [Design goals](#design-goals)
- [Layout at a glance](#layout-at-a-glance)
- [The 64-bit word & tagging scheme](#the-64-bit-word--tagging-scheme)
- [Address encoding](#address-encoding)
- [Register file](#register-file)
- [Control registers](#control-registers)
- [Instruction format](#instruction-format)
- [Opcodes](#opcodes)
- [Validation & bytecode](#validation--bytecode)
- [Worked example](#worked-example)
- [Static guarantees](#static-guarantees)
- [Building / testing](#building--testing)
- [Where next](#where-next)

## Quick start

`Lacooda64.hpp` is the umbrella header; include it and put the `lacooda/`
folder on your include path:

```cpp
#include "Lacooda64.hpp"

using namespace openjoey::lacooda64;

// Build a small program with the high-level builders.
Trace program = {
    Count(V(0), Zone(0, 1, Self)),          // count cards in a zone
    Compare(F(0), V(0), Imm(3), CompareOp::GreaterEqual),
    JumpIf(F(0), 4),                        // jump to instruction #4
    Halt(),
    // ...label "effect":
    Move(Player(0, example::ATTR_LP), Imm(500), MoveMethod::Relocate),
    Halt(),
};

// Encode to a portable word stream, or compact 32-byte little-endian bytes.
ProgramWords words = encode(program);
Bytecode     bytes = encodeBytes(program);

// Decode back (also validates every instruction along the way).
DecodeResult dr = decode(words);
```

## Design goals

- **One common currency.** A single `Word` (`std::uint64_t`) carries tag +
  payload, so addresses, registers, immediates, control selectors and operation
  bundles are all just words. This makes serialization trivial and uniform.
- **Fixed, cheap instructions.** Every instruction is exactly four words
  (32 bytes) and trivially copyable — no variable-length decoding.
- **Self-describing payloads.** The tag on each operand determines how its
  payload is interpreted, which keeps validation and dispatch simple.
- **Layering.** Each header depends only on lower layers; the dependency order
  is enforced by the include order in `Lacooda64.hpp`:
  `Word → Address → Register → Immediate → Control → Opcode → Operation →
  Instruction → Operand → Validate → Machine → Builder → Example`.

## Layout at a glance

```
Lacooda64.hpp              Umbrella header + static_asserts (entry point)
lacooda/
├── Word.hpp               Word type, aliases, tag + tagged-word helpers
├── Address.hpp            64-bit address word (level|player|zone|slot|card|attr)
├── Register.hpp           Register word (bank|index) + register banks
├── Immediate.hpp          Signed 60-bit immediate literal
├── Control.hpp            Control registers + ControlState array
├── Opcode.hpp             Opcodes + subcode enums + instruction flags
├── Operation.hpp          Operation word packs opcode+sub+flags+cause+aux
├── Instruction.hpp        Fixed 4-word instruction + slot accessors
├── Operand.hpp            Operand classification predicates
├── Validate.hpp           Operand/instruction and trace validation
├── Machine.hpp            Register files, machine state, encode/decode
├── Builder.hpp            High-level instruction constructors
└── Example.hpp            Worked example program + assembly listing
```

## The 64-bit word & tagging scheme

A `Word` (`std::uint64_t`) is split into:

```
 63                       60 59                          0
+-------------------------+-----------------------------+
|  tag (4 bits)            |  payload (60 bits)           |
+-------------------------+-----------------------------+
```

- `WordTag` (top 4 bits) identifies the *kind* of value:
  `None`, `Address`, `Register`, `Immediate`, `Control`, `Operation`, `Label`,
  `Reserved7`.
- The low 60 bits hold the payload, interpreted according to the tag.
- Helpers: `makeTagged`, `tagOf`, `payloadOf`, `isTag`.
- `None` is the canonical empty word (a fully-zero `WordTag::None` word).

```
Address            = Word          Operand            = Word
OperationWord      = Word         PlayerId           = Word
ZoneId             = Word         SlotId             = Word
CardInstanceId     = Word         CardCode           = Word
AttributeId        = Word         PhaseId/StepId/ChainId/EffectId = Word
ProgramCounter     = Word         RegisterIndex      = Word
```

## Address encoding

An `Address` packs a *location* in the duel hierarchy into the 60-bit payload:

```
payload bit
 59..53 52..50 49..48 47..42 41..36 35..12 11..0
+-------+-------+-----+------+------+-------+------------+
| resrv | level | p   | zone | slot | card  | attribute  |
+-------+-------+-----+------+------+-------+------------+
  7b      3b     2b     6b     6b    24b      12b
```

The full word adds `WordTag::Address` in bits 63–60.

**Address depth** (`AddressLevel`):

| Level     | Meaning                 | Fields used                  |
|-----------|-------------------------|------------------------------|
| `Duel`    | Whole-duel scope        | `[attribute]`                |
| `Player`  | A player                | `[p, attribute]`             |
| `Zone`    | A zone within a player  | `[p, z, attribute]`          |
| `Slot`    | A slot within a zone    | `[p, z, sl, attribute]`      |
| `Card`    | A card instance in a slot | `[p, z, sl, c, attribute]` |

Constructors `Duel`, `Player`, `Zone`, `Slot`, `Card` build addresses at each
depth. `attribute == Self` (`0`) means "the object itself"; a non-`Self`
attribute addresses a specific property *of* the object.

Predicates:
- `isObject(a)` — points at a whole object (`attribute == Self`).
- `isAttribute(a)` — points at a single attribute of an object.
- `validAddress(a)` — address-tagged with a supported level.

Helpers `makeAddress`, `withAttribute`, `levelOf`, `playerOf`, `zoneOf`,
`slotOf`, `cardOf`, `attributeOf` encode/decode the packed fields.

## Register file

Registers pack a **bank** (2 bits) and an **index** (8 bits) into a
`WordTag::Register` operand:

| Bank      | Accessor | Use                          |
|-----------|----------|------------------------------|
| `Value`   | `V(i)`   | General-purpose value words  |
| `Address` | `A(i)`   | Address/pointer words        |
| `Flag`    | `F(i)`   | Boolean 0/1 flag words       |

`Reg(bank, index)` builds one directly; `registerBank`/`registerIndex` decode.

## Control registers

`ControlReg` selects the engine's global state, stored in a fixed
`ControlState` array indexed by the enum value:

| Reg              | Meaning                                |
|------------------|----------------------------------------|
| `Turn`           | Current turn number                    |
| `Phase`          | Current phase                          |
| `Step`           | Current sub-step within a phase        |
| `Chain`          | Current chain layer                    |
| `Effect`         | Current effect / frame                 |
| `ProgramCounter` | Index into the trace of next instruction |

`Control(r)` builds the operand; `controlReg`/`controlIndex` decode it.

## Instruction format

A **fixed** instruction is exactly four 64-bit words = 32 bytes, trivially
copyable and contiguous:

```
slot [0]  operation word = opcode + subcode + flags + cause + aux
slot [1]  destination operand
slot [2]  source operand 0
slot [3]  source operand 1
```

The **operation word** payload is bit-packed with accumulative low→high shifts:

```
 59..52 opcode   (8)
 51..40 subcode  (12)
 39..28 flags    (12)
 27..20 cause    (8)
 19.. 4 aux      (16)
  3.. 0 reserved (4)
```

`Op(...)` packs an operation word; `opcodeOf`, `subcodeOf`, `flagsOf`,
`causeOf`, `auxOf` unpack it. Some opcodes reuse `subcode`:
* `Summon` splits subcode into 8-bit `SummonMethod` + 4-bit `SummonMode`
  (`summonSubcode` / `summonMethod` / `summonMode`);
* most other opcodes use `subcode(E)` to store an enum directly.

`makeInstruction(op, dst, src0, src1)` assembles an instruction;
`operation`, `dst`, `src0`, `src1` read its slots.

## Opcodes

`Opcode` names each instruction family (see `Opcode.hpp`). Key ones:

| Opcode                        | Purpose                                  |
|-------------------------------|------------------------------------------|
| `Nop` / `Halt`                | Flow control                             |
| `Set` / `Copy`                | `dst = src0` (writable dest)             |
| `Load` / `Store`              | Between registers and addressable state  |
| `Swap` / `Select`             | Exchange / pick a location               |
| `Count`                       | Count cards in a container into a reg    |
| `Move`                        | Move/relocate cards (subcode = `MoveMethod`) |
| `Summon`                      | Summon a card (subcode = method+mode)    |
| `Position` / `Equip` / `Counter` / `Control` / `Negate` / `Restrict` | Game effects |
| `Alu` / `Compare`             | Integer math / flags                     |
| `Jump` / `JumpIf`             | Control flow (subcode = `JumpCondition`) |
| `Damage` / `GainLP` / `PayLP` | Life-point changes                       |
| `Random`                      | Random events (subcode = `RandomKind`)   |
| `Event`                       | Emit an engine event (subcode = `EventKind`) |
| `Chain`                       | Chain-resolution protocol (subcode = `ChainOp`) |

Instruction **flags** (`InstructionFlag`) are combinable bit flags:
`Flag_Forced`, `Flag_Optional`, `Flag_Cost`, `Flag_Replacement`,
`Flag_Public`, `Flag_Hidden`, `Flag_Mandatory`.

Each opcode declares which side of an operand (`dst`/`src0`/`src1`) must be
present, writable or a particular bank — enforced by `validInstruction`.

## Validation & bytecode

- `validOperand(o)` — is `o` a well-formed operand?
- `validInstruction(i)` — operation word is tagged `Operation` **and** the
  operands satisfy that opcode's rules.
- `validateTrace(trace)` — validates a whole program: every instruction is
  well-formed and every `Jump`/`JumpIf` target is a legal instruction index.
  Returns a `ValidationResult` (`None` / `InvalidInstruction` /
  `JumpOutOfRange`) plus the offending index.
- `encode(trace)` → flat `ProgramWords` (4 words/instruction).
- `decode(words)` → `DecodeResult` (re-validates on the way in; reports
  `MisalignedWordCount` or `InvalidInstruction`).
- `encodeBytes(trace)` → raw little-endian `Bytecode` (exactly 32 bytes per
  instruction).

```
Trace        = std::vector<Instruction>
ProgramWords = std::vector<Word>
Bytecode     = std::vector<std::uint8_t>
```

## Worked example

`Example.hpp` builds a six-instruction program matching this assembly:

```asm
COUNT    V0, P0.MONSTER      ; count monsters in P0's monster zone
COMPARE  F0, V0, #3          ; F0 = (V0 >= 3)
JUMPIF   F0, effect          ; if F0 == true, jump to label "effect"
HALT
effect:
MOVE     DRAW, P0.DECK, P0.HAND, #2   ; draw 2 from deck into hand
HALT
```

In the fixed 4-word machine the `MOVE` count is carried in `src1`:

```cpp
inline constexpr Instruction CountMonsters = Count(V(0), P0_MONSTERS);
inline constexpr Instruction CompareThree =
    Compare(F(0), V(0), Imm(3), CompareOp::GreaterEqual);
inline constexpr Instruction JumpToEffect = JumpIf(F(0), 4);
inline constexpr Instruction Stop = Halt();
inline constexpr Instruction DrawTwo =
    makeInstruction(
        Op(Opcode::Move, sub(MoveMethod::Draw), Flag_None, CauseKind::CardEffect),
        P0_HAND, P0_DECK, Imm(2));
```

## Static guarantees

`Lacooda64.hpp` asserts the wire format at compile time:

```cpp
static_assert(sizeof(Word) == 8,         "Word must be exactly 64 bits");
static_assert(sizeof(Instruction) == 32, "Instruction must be exactly 4 words");
static_assert(std::is_trivially_copyable<Instruction>::value,
              "Instruction must remain trivially copyable");
```

## Building / testing

The library is headers-only and needs a C++20 compiler. Include the repo root
on your include path so `#include "Lacooda64.hpp"` resolves, e.g. with GCC:

```bash
g++ -std=c++20 -Wall -Wextra -I. your_file.cpp
```

There is no build system in this repository; the headers are consumed by the
surrounding OpenJoey2 engine. A standalone smoke test that exercises the
builders, validators and encode/decode round-trips can be compiled against this
header to confirm the wire format:

```bash
g++ -std=c++20 -Wall -Wextra -I. test.cpp -o test && ./test
```

## Where next

`Lacooda64` defines *encoding* (the words) and *validation*, not execution.
The host engine owns the interpreter loop: fetch the instruction at
`ControlReg::ProgramCounter`, dispatch on `opcodeOf`, and implement the
semantics of each opcode using the helpers here. Because every instruction is
self-describing and validated, host and replay remain in lock-step without a
separate schema.
