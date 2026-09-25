#include "Scheduler.hpp"

#include <stdexcept>
namespace openjoey::lacooda64::runtime {
Word Scheduler::Register(Kind kind, Word key, Frame frame, Operand result) {
  if (next_ > static_cast<Word>(kImmediateMax)) throw std::overflow_error("registration ID exhausted");
  const Word id = next_++;
  if (IsValueRegister(result)) frame.machine.regs.value[RegisterIndexOf(result)] = Imm(static_cast<SignedWord>(id));
  frame.pending.reset();
  frame.decision.reset();
  frame.answer.reset();
  frame.halted = false;
  entries_.emplace(id, Entry{kind, key, std::move(frame)});
  return id;
}
bool Scheduler::Cancel(Word id) { return entries_.erase(id) != 0; }
std::vector<Frame> Scheduler::Due(Word time) {
  std::vector<Frame> frames;
  // Sort by due time, then registration ID, independent of caller tick size.
  std::map<std::pair<Word, Word>, Frame> ready;
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.kind == Kind::kTime && it->second.key <= time) {
      ready.emplace(std::make_pair(it->second.key, it->first), it->second.frame);
      it = entries_.erase(it);
    } else
      ++it;
  }
  for (auto& [key, frame] : ready) frames.push_back(std::move(frame));
  return frames;
}
std::vector<Frame> Scheduler::Event(Word kind) const {
  std::vector<Frame> frames;
  for (const auto& [id, entry] : entries_)
    if (entry.kind == Kind::kEvent && entry.key == kind) frames.push_back(entry.frame);
  return frames;
}
}  // namespace openjoey::lacooda64::runtime
