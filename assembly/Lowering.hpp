#pragma once
#include <functional>
#include <vector>

#include "lacooda/Builder.hpp"
#include "lacooda/Validate.hpp"
namespace openjoey::lacooda64::assembly {
struct FieldCondition {
  AttributeId field;
  CompareOp comparison;
  Operand value;
};
// Builds instruction sequences from numeric operands. This layer allocates
// scratch registers and patches branches; it never installs runtime callbacks.
class Lowering {
 public:
  Operand Value();
  Operand AddressRegister();
  Operand Flag();
  Word Emit(const Instruction& instruction);
  Word Here() const { return code_.size(); }
  void PatchJump(Word instruction, Word target);
  Operand Filter(Operand container, const std::vector<FieldCondition>& conditions);
  Operand DrawToSize(Operand hand, Operand deck, Operand size);
  void RandomMove(Operand source, Operand destination, Operand count, MoveMethod method);
  void OnceOnEvent(Operand event, const std::function<void(Lowering&)>& body);
  // Append HALT and check all operand forms/targets before publishing code.
  Trace Finish();

 private:
  Word next_value_ = 0, next_address_ = 0, next_flag_ = 0;
  Trace code_;
};
}  // namespace openjoey::lacooda64::assembly
