#include "runtime/Frame.hpp"

#include <utility>

namespace openjoey::lacooda64::runtime {
Frame::Frame(Trace code) : program(std::make_shared<const Trace>(std::move(code))) {}
bool Frame::Answer(Word index) {
  if (!decision || decision->pc != pc || index >= decision->count || answer) return false;
  answer = index;
  return true;
}
}  // namespace openjoey::lacooda64::runtime
