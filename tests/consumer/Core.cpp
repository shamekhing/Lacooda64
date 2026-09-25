#include "Lacooda64.hpp"
using namespace openjoey::lacooda64;
static_assert(ValidInstruction(Set(V(0), Imm(42))));
Trace ConsumerTrace() {
  Trace trace{Set(V(0), Imm(42)), Halt()};
  auto decoded = Decode(Encode(trace));
  if (!decoded || !ValidateTrace(decoded.trace_) || EncodeBytes(trace).size() != 64) return {};
  return decoded.trace_;
}
