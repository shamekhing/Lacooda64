# Runtime

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

## Instructions and outcomes

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

## Randomness and deferred execution

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

## Modifiers and replacement handlers

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

## Scope and remaining work

Tests cover scalar execution, choices, indirect movement, deterministic draws,
schedules, events, modifiers, history, replacement cancellation/redirection and
error cases. Effect-level tests cover a filtered target query and Mirage of
Nightmare's draw/deferred-discard body. They do not prove complete card rules.

Original SUMMON, POSITION, EQUIP, COUNTER, CONTROL, NEGATE, RESTRICT and CHAIN
instructions are encodable but their runtime semantics remain unimplemented and
return `kUnsupported`. SWAP currently supports one register bank; ordered JUMPIF
variants require COMPARE followed by a boolean branch.

The full corpus is converted; see [bindings and source gaps](../data/README.md).
Activation/summon legality, battle/chain protocol,
visibility, continuous rules, replacement arbitration and transactional undo are
still outstanding. Suspended frames/registrations/collections/replacements do not
yet have a binary snapshot format. Copying a frame is not a full duel snapshot,
especially when pending replacements are shared.
