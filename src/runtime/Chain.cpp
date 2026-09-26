#include "runtime/Runtime.hpp"

namespace openjoey::lacooda64::runtime {
Result Runtime::RunChain(Frame& frame, const Instruction& instruction, Word budget) {
  auto fault = [&]() {
    frame.machine.control[ControlIndex(ControlReg::kResultSuccess)] = Imm(0);
    return Result{Status::kFault, frame.pc, 0, "invalid chain transition or link operand"};
  };
  if (chain.running) return fault();  // Link bodies cannot mutate the stack they execute on.
  const auto method = static_cast<ChainOp>(SubcodeOf(Operation(instruction)));
  const auto subject = Dst(instruction), entry = Src0(instruction);
  switch (method) {
    case ChainOp::kBegin:
      if (chain.active) return fault();
      chain = {};
      chain.active = true;
      break;
    case ChainOp::kPush: {
      if (!chain.active || chain.resolving) return fault();
      Word value;
      if (IsNone(subject) || !ReadOperand(frame, state_, subject, value)) return fault();
      ChainLink link;
      link.subject = value;
      if (!IsNone(entry)) {
        Word target;
        if (!ReadOperand(frame, state_, entry, target) || !IsImmediate(target) || ImmediateValue(target) < 0 || Word(ImmediateValue(target)) >= frame.program->size()) return fault();
        link.execution = std::make_shared<Frame>(frame);
        link.execution->pc = Word(ImmediateValue(target));
        link.execution->halted = false;
        link.execution->pending.reset();
        link.execution->decision.reset();
        link.execution->answer.reset();
      }
      chain.links.push_back(std::move(link));
      chain.passes = 0;
      Deliver({Sub(EventKind::kActivate), value, kNone, Imm(SignedWord(chain.links.size()))});
      break;
    }
    case ChainOp::kPass:
      if (!chain.active || chain.resolving || chain.passes == Word(kImmediateMax)) return fault();
      ++chain.passes;
      break;
    case ChainOp::kBeginResolve:
      if (!chain.active || chain.resolving || chain.links.empty()) return fault();
      chain.resolving = true;
      Deliver({Sub(EventKind::kResolveBegin), chain.links.back().subject, kNone, Imm(SignedWord(chain.links.size()))});
      break;
    case ChainOp::kResolveLink: {
      if (!chain.active || !chain.resolving || chain.links.empty() || chain.links.back().resolved) return fault();
      auto& link = chain.links.back();
      Word steps = 0;
      if (!link.negated && link.execution) {
        if (frame.answer) {
          if (!frame.decision || !link.execution->Answer(*frame.answer)) return fault();
          frame.answer.reset();
          frame.decision.reset();
        }
        chain.running = true;
        Result result;
        try {
          result = Run(*link.execution, budget);
        } catch (...) {
          chain.running = false;
          throw;
        }
        chain.running = false;
        steps = result.steps;
        if (result.status == Status::kWaiting && link.execution->decision) {
          frame.decision = *link.execution->decision;
          frame.decision->pc = frame.pc;
        }
        if (result.status != Status::kHalted) return {result.status, frame.pc, steps, result.message};
      }
      frame.decision.reset();
      frame.answer.reset();
      link.resolved = true;
      Deliver({Sub(EventKind::kResolveEnd), link.subject, kNone, Imm(link.negated ? 0 : 1)});
      frame.machine.control[ControlIndex(ControlReg::kChain)] = Imm(SignedWord(chain.links.size()));
      return {Status::kHalted, frame.pc, steps, {}};
    }
    case ChainOp::kPop:
      if (!chain.active || !chain.resolving || chain.links.empty() || !chain.links.back().resolved) return fault();
      chain.links.pop_back();
      break;
    case ChainOp::kEndResolve:
      if (!chain.active || !chain.resolving || !chain.links.empty()) return fault();
      chain.resolving = false;
      chain.passes = 0;
      break;
    case ChainOp::kEnd:
      if (!chain.active || chain.resolving || !chain.links.empty()) return fault();
      chain = {};
      break;
    default:
      return fault();
  }
  frame.machine.control[ControlIndex(ControlReg::kChain)] = Imm(SignedWord(chain.links.size()));
  return {Status::kHalted, frame.pc, 0, {}};
}
}  // namespace openjoey::lacooda64::runtime
