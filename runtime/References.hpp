#pragma once
#include "Frame.hpp"
#include "StateAccess.hpp"
namespace openjoey::lacooda64::runtime {
// Resolve operands without embedding project pointers in bytecode.
bool ReadOperand(Frame& frame, StateAccess& state, Operand operand, Word& value);
bool ResolveAddress(Frame& frame, Operand operand, Address& address);
bool WriteOperand(Frame& frame, StateAccess& state, Operand operand, Word value);
}  // namespace openjoey::lacooda64::runtime
