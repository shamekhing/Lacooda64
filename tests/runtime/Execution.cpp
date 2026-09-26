#include <iostream>

#include "Lacooda64.hpp"
#include "runtime/Runtime.hpp"
#include "tests/support/MemoryState.hpp"
using namespace openjoey::lacooda64;
using namespace openjoey::lacooda64::runtime;
int main() {
  bool ok = true;
  auto check = [&](bool pass, const char* message) {
    if (!pass) {
      std::cerr << message << '\n';
      ok = false;
    }
  };
  test::MemoryState state;
  std::mt19937 rng(0);
  Runtime runtime(state, rng);
  Frame arithmetic({Set(V(0), Imm(9)), Alu(V(1), V(0), Imm(3), AluOp::kDivide), Compare(F(0), V(1), Imm(3)), JumpIf(F(0), 5), Jump(4), Halt()});
  check(runtime.Run(arithmetic).status == Status::kHalted, "arithmetic/branch execution");
  Frame zero({Alu(V(0), Imm(1), Imm(0), AluOp::kDivide), Halt()});
  check(runtime.Run(zero).status == Status::kFault && zero.pc == 0, "division by zero");
  Frame overflow({Alu(V(0), Imm(kImmediateMax), Imm(2), AluOp::kMultiply), Halt()});
  check(runtime.Run(overflow).status == Status::kFault, "overflow must not wrap");
  Frame loop({Jump(0)});
  check(runtime.Run(loop, 7).status == Status::kStepLimit, "execution budget");

  state.zones[Zone(0, 1)] = {Card(0, 1, 0, 10), Card(0, 1, 1, 11)};
  state.zones[Zone(0, 2)] = {};
  Frame select({Select(A(0), Zone(0, 1)), Enumerate(V(0), A(0)), Choose(A(1), V(0), Imm(0)), Select(A(2), Zone(0, 2)), Move(A(2), A(1), MoveMethod::kRelocate), Halt()});
  check(runtime.Run(select).status == Status::kWaiting, "choice suspends");
  check(state.zones[Zone(0, 2)].empty(), "waiting must not mutate");
  check(!select.Answer(2) && select.Answer(1) && !select.Answer(0), "choice domain and single answer");
  check(runtime.Run(select).status == Status::kHalted, "choice resumes");
  check(state.zones[Zone(0, 2)].size() == 1 && CardOf(state.zones[Zone(0, 2)][0]) == 11, "indirect MOVE selected card");
  Frame number({Choose(V(0), Imm(3), Imm(1)), Halt()});
  check(runtime.Run(number).status == Status::kWaiting && number.Answer(2), "numeric choice");
  check(runtime.Run(number).status == Status::kHalted && ImmediateValue(number.machine.regs.value[0]) == 2, "numeric answer");

  std::mt19937 r1(1234), r2(1234);
  Runtime first(state, r1), second(state, r2);
  Frame random1({Random(V(0), Imm(6)), Random(V(1), Imm(100)), Halt()}), random2 = random1;
  check(first.Run(random1).status == Status::kHalted && second.Run(random2).status == Status::kHalted && random1.machine.regs.value == random2.machine.regs.value, "seeded reproducibility");
  auto saved = r1;
  Frame recorded({RecordedRandom(V(0), RandomKind::kDie, Imm(5)), Halt()});
  check(first.Run(recorded).status == Status::kHalted && r1 == saved, "recorded outcome consumes no RNG");

  const auto property = Player(0, 1);
  state.fields[property] = Imm(100);
  Frame modifiers({Modify(V(0), property, Imm(20), ModifierOp::kAdd), Modify(V(1), property, Imm(5), ModifierOp::kAdd), Schedule(V(2), 5, Imm(10)), Halt(), Halt(), Unmodify(V(0)), Halt()});
  check(runtime.Run(modifiers).status == Status::kHalted, "register modifiers and expiry");
  Word value = 0;
  check(runtime.modifiers.Read(state, property, value) && ImmediateValue(value) == 125, "stacked modifiers");
  state.fields[property] = Imm(200);
  runtime.Advance(9);
  check(runtime.TakeReady().empty(), "schedule not early");
  runtime.Advance(10);
  auto due = runtime.TakeReady();
  check(due.size() == 1, "schedule due once");
  if (!due.empty()) check(runtime.Run(due.front()).status == Status::kHalted, "captured handle expiry");
  check(runtime.modifiers.Read(state, property, value) && ImmediateValue(value) == 205, "expiry preserves changed base and other modifier");
  runtime.Advance(11);
  check(runtime.TakeReady().empty(), "schedule consumed");

  Frame subscribe({Subscribe(V(0), 2, Imm(Sub(EventKind::kTurnBegin))), Halt(), Load(V(1), Control(ControlReg::kEventContext)), Cancel(V(0)), Halt()});
  check(runtime.Run(subscribe).status == Status::kHalted, "subscription registration");
  runtime.Deliver({Sub(EventKind::kTurnBegin), Player(0), kNone, Imm(42)});
  auto events = runtime.TakeReady();
  check(events.size() == 1, "event matches subscription");
  if (!events.empty()) check(runtime.Run(events.front()).status == Status::kHalted && ImmediateValue(events.front().machine.regs.value[1]) == 42, "event context and self cancellation");
  runtime.Deliver({Sub(EventKind::kTurnBegin), Player(0), kNone, Imm(43)});
  check(runtime.TakeReady().empty(), "cancelled subscription");
  const auto before = state.zones;
  Frame unpaid({MakeInstruction(Op(Opcode::kMove, Sub(MoveMethod::kTribute), kFlagCost), Zone(0, 2), Zone(0, 1), Imm(99)), Halt()});
  check(runtime.Run(unpaid).status == Status::kFault && state.zones == before, "unpayable movement cost does not partially mutate");
  Frame bad_method({MakeInstruction(Op(Opcode::kMove, 999), Zone(0, 2), Zone(0, 1)), Halt()});
  check(runtime.Run(bad_method).status == Status::kInvalidProgram && state.zones == before, "invalid move method rejected before mutation");
  Frame unknown({Summon(Slot(0, 1, 0), Card(0, 2, 0, 100), SummonMethod::kSpecial), Halt()});
  check(runtime.Run(unknown).status == Status::kFault && state.zones == before, "unbound state layout fails without mutation");
  Frame history_read({History(V(0), HistoryField::kCount), History(V(1), HistoryField::kKind, Imm(0)), Halt()});
  check(runtime.Run(history_read).status == Status::kHalted && ImmediateValue(history_read.machine.regs.value[0]) == SignedWord(runtime.history.size()), "numeric event history access");
  // Register a replacement body that redirects one staged movement.
  Frame redirect({Subscribe(V(0), 2, Imm(Sub(EventKind::kWouldMove))), Halt(), Store(Control(ControlReg::kPendingDestination), Zone(0, 1)), Halt()});
  check(runtime.Run(redirect).status == Status::kHalted, "replacement registration");
  state.zones[Zone(0, 5)] = {Card(0, 5, 0, 77)};
  Frame staged({Stage(V(0), 3, Imm(Sub(EventKind::kWouldMove))), Commit(V(0)), Halt(), Move(Zone(0, 2), Card(0, 5, 0, 77), MoveMethod::kReturn), Halt()});
  check(runtime.Run(staged).status == Status::kWaiting && state.zones[Zone(0, 5)].size() == 1, "commit waits before mutation");
  auto handlers = runtime.TakeReady();
  check(handlers.size() == 1, "staged operation has a replacement handler");
  if (!handlers.empty()) check(runtime.Run(handlers.front()).status == Status::kHalted, "replacement updates numeric destination");
  check(runtime.Run(staged).status == Status::kHalted && state.zones[Zone(0, 5)].empty() && CardOf(state.zones[Zone(0, 1)].back()) == 77, "commit applies redirected instruction once");
  const auto committed = state.zones;
  check(runtime.Run(staged).status == Status::kHalted && state.zones == committed, "completed commit cannot repeat");
  Frame prevent({Subscribe(V(0), 2, Imm(88)), Halt(), Store(Control(ControlReg::kPendingCancelled), Imm(1)), Halt()});
  check(runtime.Run(prevent).status == Status::kHalted, "prevent registration");
  Frame cancelled({Stage(V(0), 3, Imm(88)), Commit(V(0)), Halt(), Move(Zone(0, 2), Card(0, 1, 0, 77), MoveMethod::kDestroy), Halt()});
  check(runtime.Run(cancelled).status == Status::kWaiting, "prevent pending");
  for (auto& handler : handlers = runtime.TakeReady()) check(runtime.Run(handler).status == Status::kHalted, "prevent handler");
  check(runtime.Run(cancelled).status == Status::kHalted && state.zones == committed, "cancelled destruction makes no state change");
  Frame optional_prevent({Subscribe(V(0), 2, Imm(87)), Halt(), Choose(V(1), Imm(2), Imm(0)), Compare(F(0), V(1), Imm(1)), JumpIf(F(0), 6, JumpCondition::kFalse), Store(Control(ControlReg::kPendingCancelled), Imm(1)), Halt()});
  check(runtime.Run(optional_prevent).status == Status::kHalted, "optional replacement registration");
  Frame optional_action({Stage(V(0), 3, Imm(87)), Commit(V(0)), Halt(), Move(Zone(0, 2), Card(0, 1, 0, 77), MoveMethod::kDestroy), Halt()});
  check(runtime.Run(optional_action).status == Status::kWaiting, "optional action waits");
  auto optional_handlers = runtime.TakeReady();
  check(optional_handlers.size() == 1, "optional replacement queued");
  if (!optional_handlers.empty()) {
    check(runtime.Run(optional_handlers.front()).status == Status::kWaiting, "replacement awaits player input");
    check(runtime.Run(optional_action).status == Status::kWaiting && state.zones == committed, "commit cannot bypass suspended replacement");
    check(optional_handlers.front().Answer(1) && runtime.Run(optional_handlers.front()).status == Status::kHalted, "replacement resumes after choice");
    check(runtime.Run(optional_action).status == Status::kHalted && state.zones == committed, "chosen cancellation applied");
  }
  Frame failed_action({Store(Control(ControlReg::kResultSuccess), Imm(0)), Compare(F(0), Control(ControlReg::kResultSuccess), Imm(0)), Halt()});
  check(runtime.Run(failed_action).status == Status::kHalted && failed_action.machine.regs.flag[0] == Imm(1), "explicit failed action result survives STORE bookkeeping");
  return ok ? 0 : 1;
}
