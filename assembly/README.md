# Assembly and lowering

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

See [runtime contracts](../runtime/README.md) for new primitive syntax.
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

## Numeric lowering helpers

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
[corpus guide](../data/README.md) for bindings, regeneration and source gaps.

## CLI

```sh
cmake -S . -B build -DLACOODA64_BUILD_TESTS=ON -DLACOODA64_BUILD_TOOLS=ON
cmake --build build
build/lacooda-asm input.lasm output.lasm
ctest --test-dir build --output-on-failure
```

The optional output is canonical text. Existing Encode/EncodeBytes provide word
and little-endian byte serialization of the resulting trace.
