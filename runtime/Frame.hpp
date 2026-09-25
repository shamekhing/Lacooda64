#pragma once
#include <memory>

#include "Collections.hpp"
#include "Decisions.hpp"
#include "Replacements.hpp"
#include "lacooda/Machine.hpp"
namespace openjoey::lacooda64::runtime {
struct Frame {
  std::shared_ptr<const Trace> program;
  MachineState machine{};
  Collections collections;
  Word pc{};
  std::optional<Decision> decision;
  std::optional<Word> answer;  // Candidate index, accepted only at the waiting PC.
  std::shared_ptr<PendingParticipant> pending;
  bool halted{};
  explicit Frame(Trace code) : program(std::make_shared<const Trace>(std::move(code))) {}
  bool Answer(Word index) {
    if (!decision || decision->pc != pc || index >= decision->count || answer) return false;
    answer = index;
    return true;
  }
};
}  // namespace openjoey::lacooda64::runtime
