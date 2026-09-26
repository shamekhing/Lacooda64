#include "lacooda/Validate.hpp"

namespace openjoey::lacooda64 {
ValidationResult ValidateTrace(const Trace& trace) noexcept {
  for (Word pc = 0; pc < trace.size(); ++pc) {
    const auto& i = trace[static_cast<std::size_t>(pc)];
    if (!ValidInstruction(i)) return {ValidationError::kInvalidInstruction, pc};

    const auto op = OpcodeOf(Operation(i));
    if (op == Opcode::kJump || op == Opcode::kStage || op == Opcode::kSchedule || op == Opcode::kSubscribe) {
      const SignedWord t = ImmediateValue(Src0(i));
      if (t < 0 || static_cast<Word>(t) >= trace.size()) return {ValidationError::kJumpOutOfRange, pc};
    }
    if (op == Opcode::kChain && SubcodeOf(Operation(i)) == Sub(ChainOp::kPush) && IsImmediate(Src0(i))) {
      const auto target = ImmediateValue(Src0(i));
      if (target < 0 || static_cast<Word>(target) >= trace.size()) return {ValidationError::kJumpOutOfRange, pc};
    }
    if (op == Opcode::kJumpIf) {
      const SignedWord t = ImmediateValue(Src1(i));
      if (t < 0 || static_cast<Word>(t) >= trace.size()) return {ValidationError::kJumpOutOfRange, pc};
    }
  }
  return {};
}
}  // namespace openjoey::lacooda64
