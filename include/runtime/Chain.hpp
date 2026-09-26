#pragma once
#include <memory>
#include <vector>

#include "Frame.hpp"
namespace openjoey::lacooda64::runtime {
struct ChainLink {
  Word subject{};
  std::shared_ptr<Frame> execution;
  bool negated{}, resolved{};
};
struct ChainState {
  bool active{}, resolving{}, running{};
  Word passes{};
  std::vector<ChainLink> links;
};
}  // namespace openjoey::lacooda64::runtime
