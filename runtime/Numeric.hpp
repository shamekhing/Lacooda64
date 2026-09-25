#pragma once
#include <algorithm>

#include "lacooda/Immediate.hpp"
#include "lacooda/Opcode.hpp"
namespace openjoey::lacooda64::runtime {
inline bool Arithmetic(AluOp op, SignedWord a, SignedWord b, SignedWord& out) {
  switch (op) {
    case AluOp::kAdd:
      out = a + b;
      break;
    case AluOp::kSubtract:
      out = a - b;
      break;
    case AluOp::kMultiply: {
      const Word aa = a < 0 ? Word(-a) : Word(a), bb = b < 0 ? Word(-b) : Word(b);
      const bool negative = (a < 0) != (b < 0);
      const Word limit = negative ? Word(-kImmediateMin) : Word(kImmediateMax);
      if (bb && aa > limit / bb) return false;
      out = negative ? -SignedWord(aa * bb) : SignedWord(aa * bb);
      break;
    }
    case AluOp::kDivide:
      if (!b) return false;
      out = a / b;
      break;
    case AluOp::kModulo:
      if (!b) return false;
      out = a % b;
      break;
    case AluOp::kMinimum:
      out = std::min(a, b);
      break;
    case AluOp::kMaximum:
      out = std::max(a, b);
      break;
    case AluOp::kNegate:
      out = -a;
      break;
    case AluOp::kAbsolute:
      out = a < 0 ? -a : a;
      break;
    default:
      return false;
  }
  return out >= kImmediateMin && out <= kImmediateMax;
}

}  // namespace openjoey::lacooda64::runtime
