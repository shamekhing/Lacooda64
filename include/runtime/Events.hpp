#pragma once
#include <vector>

#include "lacooda/Instruction.hpp"
namespace openjoey::lacooda64::runtime {
struct Event {
  Word kind;
  Word subject;
  Word object;
  Word context;
};
// A numeric event record, not an interpreted predicate or action string.
using EventHistory = std::vector<Event>;
}  // namespace openjoey::lacooda64::runtime
