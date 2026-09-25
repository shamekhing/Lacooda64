#include "Modifiers.hpp"

#include <stdexcept>

#include "Numeric.hpp"
namespace openjoey::lacooda64::runtime {
Word Modifiers::Add(Address field, ModifierOp operation, SignedWord value) {
  if (next_ > Word(kImmediateMax)) throw std::overflow_error("modifier ID exhausted");
  auto id = next_++;
  entries_.emplace(id, Entry{field, operation, value});
  return id;
}
bool Modifiers::Remove(Word id) { return entries_.erase(id) != 0; }
bool Modifiers::Read(const StateAccess& state, Address field, Word& value) const {
  if (!state.Read(field, value)) return false;
  for (const auto& [id, entry] : entries_) {
    const bool same = entry.field == field || (LevelOf(entry.field) == AddressLevel::kCard && LevelOf(field) == AddressLevel::kCard && CardOf(entry.field) == CardOf(field) && AttributeOf(entry.field) == AttributeOf(field));
    if (!same) continue;
    if (!IsImmediate(value)) return false;
    SignedWord result = entry.value;
    if (entry.operation != ModifierOp::kSet && !Arithmetic(entry.operation == ModifierOp::kAdd ? AluOp::kAdd : entry.operation == ModifierOp::kMultiply ? AluOp::kMultiply : AluOp::kDivide, ImmediateValue(value), entry.value, result)) return false;
    value = Imm(result);
  }
  return true;
}
}  // namespace openjoey::lacooda64::runtime
