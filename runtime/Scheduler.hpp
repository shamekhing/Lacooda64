#pragma once
#include <map>
#include <vector>

#include "Frame.hpp"
namespace openjoey::lacooda64::runtime {
// Frame copies capture registers and collection snapshots and share immutable
// program storage. Registration order is the deterministic delivery order.
class Scheduler {
 public:
  enum class Kind { kTime, kEvent };
  struct Entry {
    Kind kind;
    Word key;
    Frame frame;
  };
  Word Register(Kind kind, Word key, Frame frame, Operand result = kNone);
  bool Cancel(Word id);
  std::vector<Frame> Due(Word time);
  std::vector<Frame> Event(Word kind) const;

 private:
  Word next_ = 1;
  std::map<Word, Entry> entries_;
};
}  // namespace openjoey::lacooda64::runtime
