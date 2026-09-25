#include "assembly/Lowering.hpp"

#include <stdexcept>
namespace openjoey::lacooda64::assembly {
namespace {
Operand Allocate(RegisterBank bank, Word& next) {
  if (next >= kRegisterCount) throw std::runtime_error("lowering exhausted scratch registers");
  return Reg(bank, next++);
}
}  // namespace
Operand Lowering::Value() { return Allocate(RegisterBank::kValue, next_value_); }
Operand Lowering::AddressRegister() { return Allocate(RegisterBank::kAddress, next_address_); }
Operand Lowering::Flag() { return Allocate(RegisterBank::kFlag, next_flag_); }
Word Lowering::Emit(const Instruction& i) {
  code_.push_back(i);
  return code_.size() - 1;
}
void Lowering::PatchJump(Word instruction, Word target) {
  if (instruction >= code_.size()) throw std::runtime_error("invalid patch location");
  auto& i = code_[instruction];
  auto op = OpcodeOf(Operation(i));
  if (op == Opcode::kJump)
    i[2] = JumpTarget(target);
  else if (op == Opcode::kJumpIf)
    i[3] = JumpTarget(target);
  else
    throw std::runtime_error("patch target is not a branch");
}
Operand Lowering::Filter(Operand container, const std::vector<FieldCondition>& conditions) {
  auto input = Value(), output = Value(), size = Value(), index = Value(), value = Value();
  auto object = AddressRegister(), field = AddressRegister(), flag = Flag();
  Emit(Enumerate(input, container));
  Emit(Enumerate(output, kNone));
  Emit(Length(size, input));
  Emit(Set(index, Imm(0)));
  auto loop = Here();
  Emit(Compare(flag, index, size, CompareOp::kGreaterEqual));
  auto done = Emit(JumpIf(flag, 0));
  Emit(At(object, input, index));
  std::vector<Word> skip;
  for (const auto& condition : conditions) {
    if (!condition.field || condition.field > kAttributeMask) throw std::runtime_error("invalid query field");
    Emit(Attribute(field, object, Imm(static_cast<SignedWord>(condition.field))));
    Emit(Load(value, field));
    Emit(Compare(flag, value, condition.value, condition.comparison));
    skip.push_back(Emit(JumpIf(flag, 0, JumpCondition::kFalse)));
  }
  Emit(Append(output, object));
  for (auto branch : skip) PatchJump(branch, Here());
  Emit(Alu(index, index, Imm(1), AluOp::kAdd));
  Emit(Jump(loop));
  PatchJump(done, Here());
  return output;
}
Operand Lowering::DrawToSize(Operand hand, Operand deck, Operand size) {
  auto count = Value();
  Emit(Count(count, hand));
  Emit(Alu(count, size, count, AluOp::kSubtract));
  Emit(Alu(count, count, Imm(0), AluOp::kMaximum));
  Emit(MakeInstruction(Op(Opcode::kMove, Sub(MoveMethod::kDraw)), hand, deck, count));
  Emit(Load(count, Control(ControlReg::kResultCount)));
  return count;
}
void Lowering::RandomMove(Operand source, Operand destination, Operand count, MoveMethod method) {
  auto remaining = Value(), available = Value(), items = Value(), index = Value();
  auto object = AddressRegister(), flag = Flag();
  Emit(Set(remaining, count));
  auto loop = Here();
  Emit(Count(available, source));
  Emit(Alu(remaining, remaining, available, AluOp::kMinimum));
  Emit(Compare(flag, remaining, Imm(0), CompareOp::kLessEqual));
  auto done = Emit(JumpIf(flag, 0));
  Emit(Enumerate(items, source));
  Emit(Random(index, available, RandomKind::kRandomCard));
  Emit(At(object, items, index));
  Emit(Move(destination, object, method));
  Emit(Load(available, Control(ControlReg::kResultCount)));
  Emit(Compare(flag, available, Imm(0)));
  auto failed = Emit(JumpIf(flag, 0));
  Emit(Alu(remaining, remaining, available, AluOp::kSubtract));
  Emit(Jump(loop));
  PatchJump(done, Here());
  PatchJump(failed, Here());
}
void Lowering::OnceOnEvent(Operand event, const std::function<void(Lowering&)>& body) {
  auto id = Value();
  const auto registration = Here();
  Emit(Subscribe(id, registration + 2, event));
  auto skip = Emit(Jump(0));
  // Cancel before running the body, so events emitted by it cannot re-enter.
  Emit(Cancel(id));
  body(*this);
  Emit(Halt());
  PatchJump(skip, Here());
}
Trace Lowering::Finish() {
  Emit(Halt());
  if (!ValidateTrace(code_)) throw std::runtime_error("invalid lowered program");
  return code_;
}
Word Lowering::Here() const { return code_.size(); }
}  // namespace openjoey::lacooda64::assembly
