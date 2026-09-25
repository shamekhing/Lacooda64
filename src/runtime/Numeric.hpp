#pragma once

#include "lacooda/Immediate.hpp"
#include "lacooda/Opcode.hpp"
namespace openjoey::lacooda64::runtime {
bool Arithmetic(AluOp op, SignedWord a, SignedWord b, SignedWord& out);

}  // namespace openjoey::lacooda64::runtime
