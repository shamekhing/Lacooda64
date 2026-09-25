#include "runtime/References.hpp"
namespace openjoey::lacooda64::runtime {
namespace {
Word* Cell(Frame& f, Operand operand) {
  if (IsControl(operand)) {
    const auto selector = ControlRegOf(operand);
    if (selector >= ControlReg::kPendingDestination && selector <= ControlReg::kPendingCancelled) {
      if (!f.pending || f.pending->completed) return nullptr;
      auto& pending = *f.pending->operation;
      if (selector == ControlReg::kPendingCancelled) return &pending.cancelled;
      return &pending.instruction[1 + Sub(selector) - Sub(ControlReg::kPendingDestination)];
    }
    const auto index = static_cast<Word>(selector);
    return index < f.machine.control.size() ? &f.machine.control[index] : nullptr;
  }
  if (!IsRegister(operand)) return nullptr;
  const auto index = RegisterIndexOf(operand);
  if (IsValueRegister(operand)) return &f.machine.regs.value[index];
  if (IsAddressRegister(operand)) return &f.machine.regs.address[index];
  if (IsFlagRegister(operand)) return &f.machine.regs.flag[index];
  return nullptr;
}
}  // namespace
bool ResolveAddress(Frame& f, Operand operand, Address& address) {
  address = operand;
  if (IsAddressRegister(operand)) address = f.machine.regs.address[RegisterIndexOf(operand)];
  return ValidAddress(address);
}
bool ReadOperand(Frame& f, StateAccess& state, Operand operand, Word& value) {
  if (auto* cell = Cell(f, operand)) {
    value = *cell;
    return !IsNone(value);
  }
  if (IsAddress(operand) && IsAttribute(operand)) return state.Read(operand, value);
  value = operand;
  return IsImmediate(operand) || IsAddress(operand);
}
bool WriteOperand(Frame& f, StateAccess& state, Operand operand, Word value) {
  if (IsAddressRegister(operand) && !ValidAddress(value)) return false;
  if (IsFlagRegister(operand) && (!IsImmediate(value) || (ImmediateValue(value) != 0 && ImmediateValue(value) != 1))) return false;
  if (auto* cell = Cell(f, operand)) {
    *cell = value;
    return true;
  }
  return IsAttribute(operand) && state.Write(operand, value);
}
}  // namespace openjoey::lacooda64::runtime
