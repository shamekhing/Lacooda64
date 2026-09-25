#pragma once
#include <map>
#include <memory>

#include "lacooda/Immediate.hpp"
#include "lacooda/Instruction.hpp"
namespace openjoey::lacooda64::runtime {
struct Frame;
struct PendingOperation {
  Instruction instruction;
  Word cancelled = Imm(0);
  Word participants{};
  bool failed{};
  std::shared_ptr<Frame> execution;
};
struct PendingParticipant {
  std::shared_ptr<PendingOperation> operation;
  bool completed{};
  void Complete(bool success) {
    if (completed) return;
    completed = true;
    if (operation->participants) --operation->participants;
    if (!success) operation->failed = true;
  }
};
class Replacements {
 public:
  Word Stage(Instruction instruction);
  std::shared_ptr<PendingOperation> Find(Word id) const;
  void Erase(Word id);

 private:
  std::map<Word, std::shared_ptr<PendingOperation>> entries_;
  Word next_ = 1;
};
}  // namespace openjoey::lacooda64::runtime
