#pragma once
#include <random>

#include "Chain.hpp"
#include "Events.hpp"
#include "Modifiers.hpp"
#include "References.hpp"
#include "Result.hpp"
#include "Scheduler.hpp"
#include "StateLayout.hpp"
namespace openjoey::lacooda64::runtime {
class Runtime {
 public:
  // The caller supplies the duel's single RNG; Lacooda neither reseeds nor
  // creates a competing random stream. Copy the RNG when snapshotting a duel.
  Runtime(StateAccess& state, std::mt19937& rng, StateLayout layout = {});
  Result Run(Frame& frame, Word budget = 100000);
  // Delivery queues captured frames; callers run them with their own budget.
  void Deliver(const Event& event);
  void Advance(Word time);
  std::vector<Frame> TakeReady();
  Replacements replacements;
  Modifiers modifiers;
  Scheduler scheduler;
  EventHistory history;
  ChainState chain;

 private:
  std::vector<Frame> ready_;
  StateAccess& state_;
  std::mt19937& rng_;
  StateLayout layout_;
  Word Bounded(Word bound);
  Result Execute(Frame& frame, Word budget);
  bool ApplyPosition(StateAccess& state, Address object, PositionOp operation);
  bool ApplyStateOperation(Frame& frame, const Instruction& instruction);
  Result RunChain(Frame& frame, const Instruction& instruction, Word budget);
};
}  // namespace openjoey::lacooda64::runtime
