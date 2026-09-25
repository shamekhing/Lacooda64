#include "Replacements.hpp"

#include <stdexcept>
namespace openjoey::lacooda64::runtime {
Word Replacements::Stage(Instruction instruction) {
  if (next_ > Word(kImmediateMax)) throw std::overflow_error("pending operation ID exhausted");
  auto pending = std::make_shared<PendingOperation>();
  pending->instruction = instruction;
  auto id = next_++;
  entries_.emplace(id, std::move(pending));
  return id;
}
std::shared_ptr<PendingOperation> Replacements::Find(Word id) const {
  auto it = entries_.find(id);
  return it == entries_.end() ? nullptr : it->second;
}
void Replacements::Erase(Word id) { entries_.erase(id); }
}  // namespace openjoey::lacooda64::runtime
