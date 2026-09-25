#pragma once
#include <optional>
#include <vector>

#include "lacooda/Word.hpp"
namespace openjoey::lacooda64::runtime {
struct Decision {
  Word pc{};
  Word player{};
  Word count{};  // Numeric choice range [0,count), or candidate count.
  std::vector<Address> candidates;
};
}  // namespace openjoey::lacooda64::runtime
