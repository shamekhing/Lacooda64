#include <algorithm>
#include <iostream>
#include <map>

#include "Lacooda64.hpp"
#include "assembly/Assembler.hpp"
#include "assembly/Disassembler.hpp"
#include "runtime/Runtime.hpp"
using namespace openjoey::lacooda64;
using namespace openjoey::lacooda64::runtime;
namespace {
StateLayout Layout() { return {1, 2, 3, 4, 5, 6, 7, {8, 9, 10, 11}}; }
// Instance-indexed fixture: address aliases remain usable after relocation.
struct Storage : StateAccess {
  std::map<Word, Address> cards;
  std::map<Address, Word> fields;
  std::map<Address, unsigned> writes;
  bool reject_exchange{};
  Word next = 100;
  static Address Key(Address a) { return LevelOf(a) == AddressLevel::kCard ? Card(0, 0, 0, CardOf(a), AttributeOf(a)) : a; }
  bool Read(Address a, Word& value) const override {
    if (!IsAttribute(a) || (LevelOf(a) == AddressLevel::kCard && !cards.contains(CardOf(a)))) return false;
    auto it = fields.find(Key(a));
    if (it == fields.end()) return false;
    value = it->second;
    return true;
  }
  bool Write(Address a, Word value) override {
    if (!IsAttribute(a) || (LevelOf(a) == AddressLevel::kCard && !cards.contains(CardOf(a)))) return false;
    fields[Key(a)] = value;
    ++writes[Key(a)];
    return true;
  }
  bool Members(Address container, std::vector<Address>& result) const override {
    if (!IsObject(container)) return false;
    result.clear();
    for (auto [id, a] : cards) {
      (void)id;
      if (LevelOf(container) == AddressLevel::kDuel || (PlayerOf(container) == PlayerOf(a) && (LevelOf(container) == AddressLevel::kPlayer || (ZoneOf(container) == ZoneOf(a) && (LevelOf(container) == AddressLevel::kZone || SlotOf(container) == SlotOf(a)))))) result.push_back(a);
    }
    std::sort(result.begin(), result.end(), [](Address a, Address b) { return SlotOf(a) < SlotOf(b); });
    return true;
  }
  bool Destination(Address dst, Word id, Address& out) const {
    for (Word slot = 0; slot < 3; ++slot) {
      if (LevelOf(dst) != AddressLevel::kZone && SlotOf(dst) != slot) continue;
      const bool occupied = std::any_of(cards.begin(), cards.end(), [&](auto pair) {
        auto a = pair.second;
        return pair.first != id && PlayerOf(a) == PlayerOf(dst) && ZoneOf(a) == ZoneOf(dst) && SlotOf(a) == slot;
      });
      if (!occupied) {
        out = Card(PlayerOf(dst), ZoneOf(dst), slot, id);
        return true;
      }
    }
    return false;
  }
  bool Relocate(Address object, Address dst, Address& moved) override {
    if (!cards.contains(CardOf(object)) || !Destination(dst, CardOf(object), moved)) return false;
    cards[CardOf(object)] = moved;
    return true;
  }
  bool Create(Word prototype, Address dst, Address& created) override {
    if (prototype != 42 || !Destination(dst, next, created)) return false;
    Add(created);
    ++next;
    return true;
  }
  bool Exchange(Address first, Address second, Address& a, Address& b) override {
    if (reject_exchange || !cards.contains(CardOf(first)) || !cards.contains(CardOf(second))) return false;
    auto x = cards.at(CardOf(first)), y = cards.at(CardOf(second));
    a = Card(PlayerOf(y), ZoneOf(y), SlotOf(y), CardOf(x));
    b = Card(PlayerOf(x), ZoneOf(x), SlotOf(x), CardOf(y));
    cards[CardOf(first)] = a;
    cards[CardOf(second)] = b;
    return true;
  }
  void Add(Address a) {
    cards[CardOf(a)] = a;
    for (Word field = 1; field <= 30; ++field) fields[Key(WithAttribute(a, field))] = Imm(0);
    fields[Key(WithAttribute(a, 1))] = Imm(1);
    fields[Key(WithAttribute(a, 2))] = Imm(1);
    fields[Key(WithAttribute(a, 4))] = fields[Key(WithAttribute(a, 5))] = Imm(SignedWord(PlayerOf(a)));
  }
  SignedWord Number(Address a) const {
    Word value{};
    return Read(a, value) && IsImmediate(value) ? ImmediateValue(value) : -999;
  }
};
}  // namespace
int main() {
  bool ok = true;
  auto check = [&](bool value, const char* message) {
    if (!value) {
      std::cerr << message << '\n';
      ok = false;
    }
  };
  std::mt19937 rng(42);
  auto run = [&](Runtime& engine, Trace trace) {
    Frame f(std::move(trace));
    return engine.Run(f).status;
  };
  for (Word method = 0; method <= Sub(SummonMethod::kToken); ++method) {
    Storage state;
    Runtime engine(state, rng, Layout());
    auto card = Card(0, method == Sub(SummonMethod::kFlip) ? 1 : 2, 0, 1);
    state.Add(card);
    if (method == Sub(SummonMethod::kFlip)) {
      state.Write(WithAttribute(card, 1), Imm(3));
      state.Write(WithAttribute(card, 2), Imm(0));
      state.Write(WithAttribute(card, 3), Imm(1));
    }
    auto source = method == Sub(SummonMethod::kToken) ? Imm(42) : card;
    check(run(engine, {Summon(Slot(0, 1, 0), source, static_cast<SummonMethod>(method)), Halt()}) == Status::kHalted, "all summon methods execute");
    Word summoned{};
    check(state.Read(Player(0, 7), summoned), "summon records last object");
    check(PlayerOf(summoned) == 0 && ZoneOf(summoned) == 1 && state.Number(WithAttribute(summoned, 1)) == 1 && state.Number(WithAttribute(summoned, 2)) == 1 && state.Number(WithAttribute(summoned, 3)) == 0, "summon location and position agree");
    check(engine.history.back().kind == Sub(EventKind::kSummoned), "summon event emitted after mutation");
  }
  for (Word mode = 0; mode <= Sub(SummonMode::kSet); ++mode) {
    Storage state;
    Runtime engine(state, rng, Layout());
    auto card = Card(0, 2, 0, 1);
    state.Add(card);
    check(run(engine, {Summon(Slot(0, 1, 0), card, SummonMethod::kSpecial, static_cast<SummonMode>(mode)), Halt()}) == Status::kHalted, "all summon modes execute");
    auto expected = mode == Sub(SummonMode::kFaceUpDefense) ? 2 : mode >= Sub(SummonMode::kFaceDownDefense) ? 3 : 1;
    check(state.Number(WithAttribute(card, 1)) == expected, "summon mode is preserved");
  }
  for (Word method = 0; method <= Sub(PositionOp::kToggle); ++method) {
    Storage state;
    Runtime engine(state, rng, Layout());
    auto card = Card(0, 1, 0, 1);
    state.Add(card);
    check(run(engine, {Position(card, static_cast<PositionOp>(method)), Halt()}) == Status::kHalted, "all position variants execute");
    const SignedWord expected[]{1, 2, 1, 4, 1, 2, 3, 2};
    auto pos = state.Number(WithAttribute(card, 1));
    check(pos == expected[method] && state.Number(WithAttribute(card, 2)) == (pos == 1 || pos == 2) && state.Number(WithAttribute(card, 3)) == (pos == 2 || pos == 3), "position and derived flags stay coherent");
  }
  {
    Storage state;
    auto a = Card(0, 1, 0, 1), b = Card(0, 1, 1, 2), equipment = Card(0, 3, 0, 3);
    for (auto c : {a, b, equipment}) state.Add(c);
    Runtime engine(state, rng, Layout());
    check(run(engine, {Equip(a, equipment), Halt()}) == Status::kHalted, "equip attach");
    check(run(engine, {Equip(b, equipment, EquipOp::kAttach), Halt()}) == Status::kFault, "attach cannot overwrite an existing link");
    check(run(engine, {Equip(b, equipment, EquipOp::kTransfer), Halt()}) == Status::kHalted, "equip transfer");
    check(run(engine, {Equip(a, equipment, EquipOp::kDetach), Halt()}) == Status::kFault, "detach checks the actual target");
    check(run(engine, {Equip(b, equipment, EquipOp::kDetach), Halt()}) == Status::kHalted && state.Number(WithAttribute(equipment, 6)) == 0, "detach clears the link");
  }
  {
    Storage state;
    auto a = Card(0, 1, 0, 1), b = Card(0, 1, 1, 2);
    state.Add(a);
    state.Add(b);
    Runtime engine(state, rng, Layout());
    auto x = WithAttribute(a, 20), y = WithAttribute(b, 20);
    check(run(engine, {Counter(x, Imm(5), CounterOp::kSet), Counter(x, Imm(2), CounterOp::kPlace), Counter(x, Imm(1), CounterOp::kRemove), TransferCounters(y, x, Imm(4)), Halt()}) == Status::kHalted && state.Number(x) == 2 && state.Number(y) == 4, "counter arithmetic and transfer conserve quantities");
    check(run(engine, {TransferCounters(y, x, Imm(3)), Halt()}) == Status::kFault && state.Number(x) == 2 && state.Number(y) == 4, "insufficient transfer has no prefix mutation");
    check(run(engine, {TransferCounters(y, x), Halt()}) == Status::kHalted && state.Number(x) == 0 && state.Number(y) == 6, "default counter transfer moves all");
    auto by_aux = MakeInstruction(Op(Opcode::kCounter, Sub(CounterOp::kPlace), 0, CauseKind::kCardEffect, 20), a, Imm(3));
    check(run(engine, {by_aux, Halt()}) == Status::kHalted && state.Number(x) == 3, "object counter selector uses numeric aux");
    state.Write(x, Imm(kImmediateMax));
    check(run(engine, {TransferCounters(x, x), Halt()}) == Status::kHalted && state.Number(x) == kImmediateMax, "self transfer cannot overflow");
    check(run(engine, {TransferCounters(WithAttribute(Card(1, 2, 2, 1), 20), x), Halt()}) == Status::kHalted && state.Number(x) == kImmediateMax, "identity aliases do not duplicate counters");
    auto restriction = WithAttribute(a, 21);
    check(run(engine, {Restrict(restriction, Imm(2)), Restrict(restriction, Imm(3), RestrictOp::kIncrement), Restrict(restriction, Imm(1), RestrictOp::kDecrement), Halt()}) == Status::kHalted && state.Number(restriction) == 4, "restriction lifecycle arithmetic");
    check(run(engine, {Restrict(restriction, Imm(5), RestrictOp::kDecrement), Halt()}) == Status::kFault && state.Number(restriction) == 4, "restriction decrement cannot underflow");
    check(run(engine, {Restrict(restriction, Imm(0), RestrictOp::kClear), Halt()}) == Status::kHalted && state.Number(restriction) == 0, "restriction clear");
  }
  {
    Storage state;
    auto card = Card(1, 1, 0, 1);
    state.Add(card);
    Runtime engine(state, rng, Layout());
    Frame take({Select(A(0), card), ChangeControl(Player(0), A(0), ControlOp::kTake), Halt()});
    check(engine.Run(take).status == Status::kHalted && PlayerOf(state.cards.at(1)) == 0 && state.Number(WithAttribute(card, 4)) == 0 && PlayerOf(take.machine.regs.address[0]) == 0, "control take moves membership and updates references");
    check(run(engine, {ChangeControl(Slot(1, 1, 1), state.cards.at(1), ControlOp::kGive), Halt()}) == Status::kHalted && SlotOf(state.cards.at(1)) == 1, "control give accepts a destination slot");
    check(run(engine, {ChangeControl(Player(0), state.cards.at(1), ControlOp::kTake), ChangeControl(Player(1), state.cards.at(1), ControlOp::kReturn), Halt()}) == Status::kHalted && state.Number(WithAttribute(card, 4)) == 1 && PlayerOf(state.cards.at(1)) == 1, "control return restores owner membership");
    auto second = Card(0, 1, 2, 2);
    state.Add(second);
    check(run(engine, {ChangeControl(second, state.cards.at(1), ControlOp::kSwap), Halt()}) == Status::kHalted && PlayerOf(state.cards.at(1)) == 0 && PlayerOf(state.cards.at(2)) == 1 && state.Number(WithAttribute(second, 4)) == 1, "control swap exchanges occupied locations");
    state.reject_exchange = true;
    auto before = state.cards;
    check(run(engine, {ChangeControl(state.cards.at(2), state.cards.at(1), ControlOp::kSwap), Halt()}) == Status::kFault && before == state.cards, "rejected exchange leaves locations unchanged");
  }
  for (Word method = 0; method <= Sub(NegateOp::kAttack); ++method) {
    Storage state;
    auto card = Card(0, 1, 0, 1);
    state.Add(card);
    Runtime engine(state, rng, Layout());
    check(run(engine, {Negate(card, static_cast<NegateOp>(method)), Halt()}) == Status::kHalted && state.Number(WithAttribute(card, 8 + method)) == 1, "every negation kind writes its own bound state");
  }
  {
    Storage state;
    Runtime engine(state, rng, Layout());
    auto assembly = assembly::Assemble(R"(
CHAIN BEGIN
SET V0, #7
CHAIN PUSH, [0:1:0:1:0], first
SET V0, #99
CHAIN PUSH, [0:1:1:2:0], second
CHAIN PASS
CHAIN BEGIN_RESOLVE
CHAIN RESOLVE_LINK
CHAIN POP
CHAIN RESOLVE_LINK
CHAIN POP
CHAIN END_RESOLVE
CHAIN END
HALT
first:
STORE [0:50], V0
HALT
second:
STORE [0:50], #2
CHOOSE V1, #2, #0
STORE [0:51], V1
HALT
)");
    check(assembly.ok(), "chain labels assemble");
    check(assembly.ok() && assembly::Assemble(assembly::Disassemble(assembly.trace)).trace == assembly.trace, "chain body target roundtrip");
    Frame f(assembly.trace);
    Result result;
    do {
      result = engine.Run(f, 3);
    } while (result.status == Status::kStepLimit);
    check(result.status == Status::kWaiting && state.Number(Player(0, 50)) == 2, "chain resolves top link first and suspends for a choice");
    check(!f.Answer(2) && f.Answer(1), "chain forwards choice validation");
    do {
      result = engine.Run(f, 3);
    } while (result.status == Status::kStepLimit);
    check(result.status == Status::kHalted && state.Number(Player(0, 50)) == 7 && state.Number(Player(0, 51)) == 1 && state.writes[Player(0, 50)] == 2 && !engine.chain.active, "chain resumes once, preserves captured registers, and closes");
    check(run(engine, {Chain(ChainOp::kPop), Halt()}) == Status::kFault, "invalid chain transition faults");
  }
  {
    Storage state;
    auto card = Card(0, 1, 0, 1);
    state.Add(card);
    Runtime engine(state, rng, Layout());
    Frame f({Chain(ChainOp::kBegin), Chain(ChainOp::kPush, card, Imm(10)), Negate(card, NegateOp::kEffect), Chain(ChainOp::kBeginResolve), Chain(ChainOp::kResolveLink), Chain(ChainOp::kPop), Chain(ChainOp::kEndResolve), Chain(ChainOp::kEnd), Halt(), Halt(), Store(Player(0, 50), Imm(99)), Halt()});
    check(engine.Run(f).status == Status::kHalted && !state.fields.contains(Player(0, 50)), "negated link body never executes");
  }
  {
    Storage state;
    auto card = Card(0, 2, 0, 1);
    state.Add(card);
    Runtime engine(state, rng, Layout());
    Frame handler({Subscribe(V(0), 2, Imm(90)), Halt(), Load(A(0), Control(ControlReg::kEventObject)), Negate(A(0), NegateOp::kSummon), Halt()});
    check(engine.Run(handler).status == Status::kHalted, "summon replacement registered");
    Frame action({Stage(V(0), 3, Imm(90)), Commit(V(0)), Halt(), Summon(Slot(0, 1, 0), card, SummonMethod::kSpecial), Halt()});
    check(engine.Run(action).status == Status::kWaiting, "summon is staged before mutation");
    for (auto& queued : engine.TakeReady()) check(engine.Run(queued).status == Status::kHalted, "summon negation cancels matching pending instruction");
    check(engine.Run(action).status == Status::kHalted && ZoneOf(state.cards.at(1)) == 2, "negated summon never relocates");
  }
  {
    Storage state;
    auto a = Card(0, 2, 0, 1), occupied = Card(0, 1, 0, 2);
    state.Add(a);
    state.Add(occupied);
    Runtime engine(state, rng, Layout());
    auto before = state.cards;
    check(run(engine, {Summon(Slot(0, 1, 0), a, SummonMethod::kSpecial), Halt()}) == Status::kFault && state.cards == before, "occupied summon slot has no movement");
    check(run(engine, {Summon(Slot(0, 1, 1), Imm(999), SummonMethod::kToken), Halt()}) == Status::kFault && state.cards == before, "unknown token prototype has no allocation");
    auto bad = Layout();
    bad.position = 0;
    Runtime unbound(state, rng, bad);
    check(run(unbound, {Position(a, PositionOp::kDefense), Halt()}) == Status::kFault, "missing field binding fails explicitly");
    check(!assembly::Assemble("CHAIN PUSH, [0:1:0:1:0], #100\nHALT").ok(), "chain target checked before execution");
    auto transfer = assembly::Assemble("COUNTER TRANSFER, [0:1:0:1:20], [0:1:1:2:20], #3\nHALT");
    check(transfer.ok() && assembly::Assemble(assembly::Disassemble(transfer.trace)).trace == transfer.trace, "counter transfer syntax and exact roundtrip");
  }
  {
    // A suspended link must propagate replacement waits and resume only once.
    Storage state;
    auto card = Card(0, 2, 0, 1);
    state.Add(card);
    Runtime engine(state, rng, Layout());
    Frame handler({Subscribe(V(0), 2, Imm(91)), Halt(), Halt()});
    check(engine.Run(handler).status == Status::kHalted, "chain replacement handler registered");
    Frame f({Chain(ChainOp::kBegin), Chain(ChainOp::kPush, card, Imm(8)), Chain(ChainOp::kBeginResolve), Chain(ChainOp::kResolveLink), Chain(ChainOp::kPop), Chain(ChainOp::kEndResolve), Chain(ChainOp::kEnd), Halt(), Stage(V(0), 11, Imm(91)), Commit(V(0)), Halt(), Summon(Slot(0, 1, 0), card, SummonMethod::kSpecial)});
    check(engine.Run(f).status == Status::kWaiting && !f.decision && ZoneOf(state.cards.at(1)) == 2, "chain replacement wait propagates before mutation");
    for (auto& queued : engine.TakeReady()) check(engine.Run(queued).status == Status::kHalted, "chain replacement handler completes");
    check(engine.Run(f).status == Status::kHalted && ZoneOf(state.cards.at(1)) == 1 && state.writes[Storage::Key(WithAttribute(card, 1))] == 1, "chain resumes staged summon exactly once");
  }
  {
    Storage state;
    Runtime engine(state, rng, Layout());
    Frame f({Chain(ChainOp::kBegin), Chain(ChainOp::kPush, Imm(1), Imm(4)), Chain(ChainOp::kBeginResolve), Chain(ChainOp::kResolveLink), Chain(ChainOp::kEnd), Halt()});
    check(engine.Run(f).status == Status::kFault && !engine.chain.running && engine.chain.links.size() == 1, "chain body cannot invalidate its executing stack");
  }
  return ok ? 0 : 1;
}
