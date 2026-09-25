#include "runtime/Runtime.hpp"

#include <algorithm>
#include <limits>

#include "Numeric.hpp"

namespace openjoey::lacooda64::runtime {
Word Runtime::Bounded(Word bound) {
  // Defined rejection sampling: unlike std::uniform_int_distribution, this
  // mapping is identical across standard library implementations.
  const Word threshold = (Word{0} - bound) % bound;
  for (;;) {
    Word sample = Word(rng_()) << 32;
    sample |= rng_();
    if (sample >= threshold) return sample % bound;
  }
}
Result Runtime::Run(Frame& f, Word budget) {
  auto result = Execute(f, budget);
  if (f.pending && result.status != Status::kWaiting && result.status != Status::kStepLimit) f.pending->Complete(result.status == Status::kHalted);
  return result;
}
Result Runtime::Execute(Frame& f, Word budget) {
  if (!f.program || !ValidateTrace(*f.program) || f.pc > f.program->size()) return {Status::kInvalidProgram, f.pc, 0, "invalid trace or program counter"};
  if (f.halted) return {Status::kHalted, f.pc, 0, {}};
  ModifiedState state(state_, modifiers);
  Word steps = 0;
  auto read = [&](Operand o, Word& w) { return ReadOperand(f, state, o, w); };
  auto number = [&](Operand o, SignedWord& v) {
    Word w;
    if (!read(o, w) || !IsImmediate(w)) return false;
    v = ImmediateValue(w);
    return true;
  };
  auto write = [&](Operand o, Word w) { return WriteOperand(f, state, o, w); };
  auto address = [&](Operand o, Address& a) { return ResolveAddress(f, o, a); };
  auto control = [&](ControlReg r, Word w) { f.machine.control[ControlIndex(r)] = w; };
  auto collection = [&](Operand o) -> std::vector<Address>* {
    Word w;
    return read(o, w) ? f.collections.Get(w) : nullptr;
  };
  while (f.pc < f.program->size()) {
    if (steps == budget) return {Status::kStepLimit, f.pc, steps, {}};
    const auto& i = (*f.program)[f.pc];
    const auto op = OpcodeOf(Operation(i));
    const auto sub = SubcodeOf(Operation(i));
    auto d = Dst(i), s = Src0(i), t = Src1(i);
    ++steps;
    control(ControlReg::kProgramCounter, Imm(static_cast<SignedWord>(f.pc)));
    Word next = f.pc + 1, w = 0, other = 0;
    SignedWord a = 0, b = 0, result = 0;
    Address dst = 0, src = 0;
    bool valid = true;
    switch (op) {
      case Opcode::kNop:
        break;
      case Opcode::kHalt:
        f.halted = true;
        return {Status::kHalted, f.pc, steps, {}};
      case Opcode::kSet:
      case Opcode::kCopy:
        valid = read(s, w) && write(d, w);
        break;
      case Opcode::kSelect:
        valid = address(s, src) && write(d, src);
        break;
      case Opcode::kLoad:
        if (IsAddressSource(s))
          valid = address(s, src) && IsAttribute(src) && state.Read(src, w) && write(d, w);
        else
          valid = read(s, w) && write(d, w);
        break;
      case Opcode::kStore:
        valid = read(s, w);
        if (valid && IsAddressRegister(d))
          valid = address(d, dst) && IsAttribute(dst) && state.Write(dst, w);
        else if (valid)
          valid = write(d, w);
        if (d != Control(ControlReg::kResultSuccess)) control(ControlReg::kResultSuccess, Imm(valid));
        break;
      case Opcode::kSwap:
        // Validate supported destinations before either write occurs.
        if (!IsRegister(d) || !IsRegister(s) || RegisterBankOf(d) != RegisterBankOf(s)) return {Status::kUnsupported, f.pc, steps, "SWAP currently requires registers in the same bank"};
        valid = read(d, w) && read(s, other) && write(d, other) && write(s, w);
        break;
      case Opcode::kAttribute:
        valid = address(s, src) && number(t, a) && a >= 0 && Word(a) <= kAttributeMask && write(d, WithAttribute(src, Word(a)));
        break;
      case Opcode::kAlu:
        valid = number(s, a) && (IsNone(t) || number(t, b)) && Arithmetic(static_cast<AluOp>(sub), a, b, result) && write(d, Imm(result));
        break;
      case Opcode::kCompare: {
        valid = read(s, w) && read(t, other);
        bool flag = false;
        if (valid && sub <= Sub(CompareOp::kNotEqual)) {
          flag = sub == Sub(CompareOp::kEqual) ? w == other : w != other;
        } else if (valid && number(s, a) && number(t, b)) {
          switch (static_cast<CompareOp>(sub)) {
            case CompareOp::kLess:
              flag = a < b;
              break;
            case CompareOp::kLessEqual:
              flag = a <= b;
              break;
            case CompareOp::kGreater:
              flag = a > b;
              break;
            case CompareOp::kGreaterEqual:
              flag = a >= b;
              break;
            default:
              valid = false;
          }
        } else
          valid = false;
        if (valid) valid = write(d, Imm(flag));
        break;
      }
      case Opcode::kJump:
        next = Word(ImmediateValue(s));
        break;
      case Opcode::kJumpIf: {
        valid = number(s, a);
        bool take = false;
        switch (static_cast<JumpCondition>(sub)) {
          case JumpCondition::kAlways:
            take = true;
            break;
          case JumpCondition::kZero:
          case JumpCondition::kFalse:
            take = a == 0;
            break;
          case JumpCondition::kNotZero:
          case JumpCondition::kTrue:
            take = a != 0;
            break;
          default:
            return {Status::kUnsupported, f.pc, steps, "use COMPARE followed by a boolean JUMPIF"};
        }
        if (take) next = Word(ImmediateValue(t));
        break;
      }
      case Opcode::kCount:
      case Opcode::kEnumerate: {
        std::vector<Address> values;
        valid = (op == Opcode::kEnumerate && IsNone(s)) || (address(s, src) && IsObject(src) && state.Members(src, values));
        if (valid)
          for (auto value : values)
            if (!ValidAddress(value) || !IsObject(value)) valid = false;
        if (valid) valid = write(d, op == Opcode::kCount ? Imm(static_cast<SignedWord>(values.size())) : f.collections.Add(std::move(values)));
        break;
      }
      case Opcode::kAt: {
        auto* values = collection(s);
        valid = values && number(t, a) && a >= 0 && Word(a) < values->size() && write(d, (*values)[Word(a)]);
        break;
      }
      case Opcode::kAppend: {
        auto* values = collection(d);
        valid = values && address(s, src) && IsObject(src);
        if (valid) values->push_back(src);
        break;
      }
      case Opcode::kLength: {
        auto* values = collection(s);
        valid = values && write(d, Imm(static_cast<SignedWord>(values->size())));
        break;
      }
      case Opcode::kChoose: {
        if (!f.decision) {
          valid = number(t, a) && a >= 0;
          if (valid && IsAddressRegister(d)) {
            auto* values = collection(s);
            valid = values && !values->empty();
            if (valid) f.decision = Decision{f.pc, Word(a), values->size(), *values};
          } else if (valid) {
            valid = number(s, b) && b > 0;
            if (valid) f.decision = Decision{f.pc, Word(a), Word(b), {}};
          }
        } else
          valid = f.decision->pc == f.pc;
        if (!valid) break;
        if (!f.answer) return {Status::kWaiting, f.pc, steps, {}};
        valid = *f.answer < f.decision->count && (!IsAddressRegister(d) || *f.answer < f.decision->candidates.size());
        if (valid) valid = write(d, IsAddressRegister(d) ? f.decision->candidates[*f.answer] : Imm(static_cast<SignedWord>(*f.answer)));
        if (valid) {
          f.decision.reset();
          f.answer.reset();
        }
        break;
      }
      case Opcode::kRandom:
        if (IsNone(s)) {
          valid = number(t, a) && a > 0;
          if (valid) valid = write(d, Imm(static_cast<SignedWord>(Bounded(Word(a)))));
        } else
          valid = read(s, w) && write(d, w);
        break;
      case Opcode::kMove: {
        std::vector<Address> values;
        valid = address(d, dst) && address(s, src) && IsObject(dst) && IsObject(src);
        a = 1;
        if (valid && !IsNone(t)) valid = number(t, a) && a >= 0;
        if (valid && LevelOf(src) == AddressLevel::kCard)
          values.push_back(src);
        else if (valid)
          valid = state.Members(src, values);
        if (valid && (FlagsOf(Operation(i)) & kFlagCost) && Word(a) > values.size()) {
          control(ControlReg::kResultCount, Imm(0));
          control(ControlReg::kResultSuccess, Imm(0));
          return {Status::kFault, f.pc, steps, "insufficient objects for cost"};
        }
        const auto count = valid ? std::min<Word>(Word(a), values.size()) : 0;
        control(ControlReg::kResultCount, Imm(0));
        for (Word n = 0; valid && n < count; ++n) {
          Address moved;
          valid = ValidAddress(values[n]) && state.Relocate(values[n], dst, moved);
          if (valid) {
            control(ControlReg::kResultCount, Imm(SignedWord(n + 1)));
            Deliver({Sub(EventKind::kMoved), values[n], moved, Operation(i)});
          }
        }
        control(ControlReg::kResultSuccess, Imm(valid));
        break;
      }
      case Opcode::kDamage:
      case Opcode::kPayLp:
      case Opcode::kGainLp:
        valid = address(d, dst) && IsAttribute(dst) && state.Read(dst, w) && IsImmediate(w) && number(s, b) && b >= 0;
        if (valid) {
          a = ImmediateValue(w);
          if (op == Opcode::kPayLp && a < b) {
            control(ControlReg::kResultSuccess, Imm(0));
            return {Status::kFault, f.pc, steps, "insufficient value for payment"};
          }
          valid = Arithmetic(op == Opcode::kGainLp ? AluOp::kAdd : AluOp::kSubtract, a, b, result) && state.Write(dst, Imm(result));
        }
        control(ControlReg::kResultSuccess, Imm(valid));
        break;
      case Opcode::kHistory:
        if (sub == Sub(HistoryField::kCount))
          valid = write(d, Imm(static_cast<SignedWord>(history.size())));
        else {
          valid = number(s, a) && a >= 0 && Word(a) < history.size();
          if (valid) {
            const auto& event = history[Word(a)];
            w = sub == Sub(HistoryField::kKind) ? Imm(static_cast<SignedWord>(event.kind)) : sub == Sub(HistoryField::kSubject) ? event.subject : sub == Sub(HistoryField::kObject) ? event.object : event.context;
            valid = write(d, w);
          }
        }
        break;
      case Opcode::kStage: {
        valid = number(t, a) && a > 0;
        if (!valid) break;
        Instruction planned = (*f.program)[Word(ImmediateValue(s))];
        const auto action = OpcodeOf(Operation(planned));
        // Only one concrete state mutation is staged, never a hidden program.
        if (action != Opcode::kMove && action != Opcode::kStore && action != Opcode::kDamage && action != Opcode::kGainLp && action != Opcode::kPayLp) return {Status::kUnsupported, f.pc, steps, "instruction cannot be staged"};
        if (action == Opcode::kStore && IsControl(Dst(planned))) return {Status::kUnsupported, f.pc, steps, "staged STORE requires an attribute destination"};
        if (IsAddressRegister(Dst(planned))) valid = address(Dst(planned), planned[1]);
        if (IsRegister(Src0(planned))) valid = valid && read(Src0(planned), planned[2]);
        if (IsRegister(Src1(planned))) valid = valid && read(Src1(planned), planned[3]);
        valid = valid && ValidInstruction(planned);
        if (valid) {
          w = replacements.Stage(planned);
          auto pending = replacements.Find(w);
          Event event{Word(a), Dst(planned), Src0(planned), Imm(static_cast<SignedWord>(w))};
          history.push_back(event);
          for (auto handler : scheduler.Event(event.kind)) {
            handler.pending = std::make_shared<PendingParticipant>();
            handler.pending->operation = pending;
            ++pending->participants;
            auto& c = handler.machine.control;
            c[ControlIndex(ControlReg::kEventKind)] = Imm(static_cast<SignedWord>(event.kind));
            c[ControlIndex(ControlReg::kEventSubject)] = event.subject;
            c[ControlIndex(ControlReg::kEventObject)] = event.object;
            c[ControlIndex(ControlReg::kEventContext)] = event.context;
            ready_.push_back(std::move(handler));
          }
          valid = write(d, Imm(static_cast<SignedWord>(w)));
        }
        break;
      }
      case Opcode::kCommit: {
        valid = number(s, a) && a > 0;
        auto pending = valid ? replacements.Find(Word(a)) : nullptr;
        valid = bool(pending);
        if (!valid) break;
        if (pending->participants) return {Status::kWaiting, f.pc, steps, "replacement handlers are still running"};
        if (pending->failed) return {Status::kFault, f.pc, steps, "replacement handler failed"};
        if (!IsImmediate(pending->cancelled) || (ImmediateValue(pending->cancelled) != 0 && ImmediateValue(pending->cancelled) != 1)) {
          valid = false;
          break;
        }
        if (ImmediateValue(pending->cancelled)) {
          control(ControlReg::kResultSuccess, Imm(0));
          control(ControlReg::kResultCount, Imm(0));
          replacements.Erase(Word(a));
          break;
        }
        if (OpcodeOf(Operation(pending->instruction)) == Opcode::kStore && !IsAttribute(Dst(pending->instruction))) return {Status::kInvalidProgram, f.pc, steps, "replacement STORE destination must remain an attribute"};
        if (!pending->execution) pending->execution = std::make_shared<Frame>(Trace{pending->instruction});
        auto applied = Run(*pending->execution, budget - steps);
        steps += applied.steps;
        if (applied.status != Status::kHalted) return {applied.status, f.pc, steps, applied.message};
        for (auto reg : {ControlReg::kResultCount, ControlReg::kResultSuccess}) control(reg, pending->execution->machine.control[ControlIndex(reg)]);
        replacements.Erase(Word(a));
        break;
      }
      case Opcode::kSchedule:
      case Opcode::kSubscribe: {
        valid = number(t, a) && a >= 0;
        if (valid) {
          Frame capture = f;
          capture.pc = Word(ImmediateValue(s));
          w = scheduler.Register(op == Opcode::kSchedule ? Scheduler::Kind::kTime : Scheduler::Kind::kEvent, Word(a), std::move(capture), d);
          valid = write(d, Imm(static_cast<SignedWord>(w)));
        }
        break;
      }
      case Opcode::kModify:
        valid = address(s, src) && IsAttribute(src) && number(t, a) && state.Read(src, w) && IsImmediate(w);
        if (valid) {
          w = modifiers.Add(src, static_cast<ModifierOp>(sub), a);
          Word effective;
          if (!modifiers.Read(state_, src, effective)) {
            modifiers.Remove(w);
            valid = false;
          } else
            valid = write(d, Imm(static_cast<SignedWord>(w)));
        }
        break;
      case Opcode::kUnmodify:
        valid = number(s, a) && a > 0;
        if (valid) control(ControlReg::kResultSuccess, Imm(modifiers.Remove(Word(a))));
        break;
      case Opcode::kCancel:
        valid = number(s, a) && a > 0;
        if (valid) control(ControlReg::kResultSuccess, Imm(scheduler.Cancel(Word(a))));
        break;
      case Opcode::kEvent:
        valid = (IsNone(d) || read(d, w)) && (IsNone(s) || read(s, other));
        {
          Word context = 0;
          valid = valid && (IsNone(t) || read(t, context));
          if (valid) Deliver({sub, w, other, context});
        }
        break;
      default:
        return {Status::kUnsupported, f.pc, steps, "opcode execution semantics not implemented"};
    }
    if (!valid)
      return {Status::kFault, f.pc, steps,
              "invalid operand value, arithmetic overflow, or state access "
              "failure"};
    f.pc = next;
  }
  f.halted = true;
  return {Status::kHalted, f.pc, steps, {}};
}
void Runtime::Deliver(const Event& event) {
  history.push_back(event);
  for (auto frame : scheduler.Event(event.kind)) {
    auto& control = frame.machine.control;
    control[ControlIndex(ControlReg::kEventKind)] = Imm(static_cast<SignedWord>(event.kind));
    control[ControlIndex(ControlReg::kEventSubject)] = event.subject;
    control[ControlIndex(ControlReg::kEventObject)] = event.object;
    control[ControlIndex(ControlReg::kEventContext)] = event.context;
    ready_.push_back(std::move(frame));
  }
}
void Runtime::Advance(Word time) {
  for (auto frame : scheduler.Due(time)) ready_.push_back(std::move(frame));
}
std::vector<Frame> Runtime::TakeReady() {
  auto ready = std::move(ready_);
  ready_.clear();
  return ready;
}
Runtime::Runtime(StateAccess& state, std::mt19937& rng) : state_(state), rng_(rng) {}
}  // namespace openjoey::lacooda64::runtime
