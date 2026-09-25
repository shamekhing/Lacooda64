#include <iostream>

#include "assembly/Assembler.hpp"
#include "assembly/Disassembler.hpp"
#include "assembly/Lowering.hpp"
#include "runtime/Runtime.hpp"
#include "tests/support/MemoryState.hpp"
using namespace openjoey::lacooda64;
using namespace openjoey::lacooda64::assembly;
using namespace openjoey::lacooda64::runtime;
int main() {
  bool ok = true;
  auto check = [&](bool value, const char* message) {
    if (!value) {
      std::cerr << message << '\n';
      ok = false;
    }
  };
  // Query: choose a face-up controlled monster with ATK <= 1000. Numeric
  // field IDs are fixtures supplied to lowering, not built into Lacooda.
  auto monsters = Zone(0, 1), deck = Zone(0, 2), hand = Zone(0, 3), grave = Zone(0, 4);
  test::MemoryState state;
  auto low = Card(0, 1, 0, 1), high = Card(0, 1, 1, 2), hidden = Card(0, 1, 2, 3);
  state.zones[monsters] = {low, high, hidden};
  for (auto c : {low, high, hidden}) {
    state.fields[WithAttribute(c, 1)] = Imm(c == high ? 2000 : 500);
    state.fields[WithAttribute(c, 2)] = Imm(c == hidden ? 0 : 1);
  }
  Lowering query;
  auto selected = query.Filter(monsters, {{1, CompareOp::kLessEqual, Imm(1000)}, {2, CompareOp::kEqual, Imm(1)}});
  auto target = query.AddressRegister();
  query.Emit(Choose(target, selected, Imm(0)));
  std::mt19937 rng(17);
  Runtime runtime(state, rng);
  Frame choose(query.Finish());
  check(runtime.Run(choose).status == Status::kWaiting && choose.decision->candidates == std::vector<Address>{low}, "compiled query has exactly the legal target");
  check(choose.Answer(0) && runtime.Run(choose).status == Status::kHalted, "compiled target choice executes");

  // data/effects.txt: Mirage of Nightmare's resolution body, lines 1617-1620.
  // The timing binder delivers the one-shot event when the next controller
  // Standby Phase occurs; that event ID is a numeric program input.
  state.zones[hand] = {Card(0, 3, 0, 10)};
  state.zones[deck] = {Card(0, 2, 0, 11), Card(0, 2, 1, 12), Card(0, 2, 2, 13), Card(0, 2, 3, 14)};
  state.zones[grave] = {};
  Lowering mirage;
  auto drawn = mirage.DrawToSize(hand, deck, Imm(4));
  mirage.OnceOnEvent(Imm(99), [&](Lowering& body) { body.RandomMove(hand, grave, drawn, MoveMethod::kDiscard); });
  const auto code = mirage.Finish();
  auto encoded = Disassemble(code);
  auto assembled = Assemble(encoded);
  check(assembled.ok() && assembled.trace == code, "lowered effect is ordinary round-trippable bytecode");
  Frame effect(assembled.trace);
  check(runtime.Run(effect).status == Status::kHalted && state.zones[hand].size() == 4 && state.zones[deck].size() == 1, "draw until four using MOVE and arithmetic");
  check(state.zones[grave].empty(), "deferred discard not immediate");
  // Later hand changes must not change the captured draw count (three).
  runtime.Deliver({99, Player(0), kNone, kNone});
  auto ready = runtime.TakeReady();
  check(ready.size() == 1, "one captured discard program");
  if (!ready.empty()) check(runtime.Run(ready.front()).status == Status::kHalted, "random discard program executes");
  check(state.zones[hand].size() == 1 && state.zones[grave].size() == 3, "exactly three distinct cards discarded");
  runtime.Deliver({99, Player(0), kNone, kNone});
  check(runtime.TakeReady().empty(), "one-shot event does not discard twice");
  return ok ? 0 : 1;
}
