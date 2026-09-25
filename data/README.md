# Effect corpus

`effects.lasm` contains **all 264 effect blocks from the 120 cards in
`effects.txt`**, converted to numbered native Lacooda instructions. Each block
has its own `.program CARD_ID_EFFECT_NUMBER` / `.end` boundary, registers and
program counter. Comments identify the original source line and explain each
instruction. `effects.txt` is unchanged.

The conversion compiles the structured `EFFECT` blocks. The cards' prose `TEXT`
paragraphs are reference material, not a second input language. Differences
between those two representations are not automatically repaired.

## Generate and check

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
| `../tools/effect_source.py` | Source parser, including nested choices and conditionals |
| `../tools/effect_compile.py` | Source-to-instruction lowering |
| `../tools/convert_effects.py` | Reproducible conversion entry point |

There are no `.constant` action records or generic effect opcodes. Queries become
`ENUMERATE`, `AT`, `ATTRIBUTE`, `LOAD`, comparisons and collection-building loops.
Decisions become `CHOOSE`; random selections use `RANDOM` with the duel's seed.
Arithmetic, state changes, delayed bodies and modifier removal are instructions,
not strings handed to an effect callback.

## Binding the programs

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

## Deferred execution and lifetimes

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

## Source gaps and execution coverage

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
[runtime coverage](../runtime/README.md) before executing other programs.
