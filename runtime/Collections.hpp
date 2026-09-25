#pragma once
#include <vector>

#include "lacooda/Address.hpp"
namespace openjoey::lacooda64::runtime {
// Owned snapshots: filtering is ordinary At/Load/Compare/Jump/Append code.
// Handles are runtime values, not literals embedded in instruction streams.
struct Collections {
  std::vector<std::vector<Address>> entries;
  Word Add(std::vector<Address> values) {
    entries.push_back(std::move(values));
    return MakeTagged(WordTag::kCollection, entries.size() - 1);
  }
  std::vector<Address>* Get(Word handle) {
    if (!IsTag(handle, WordTag::kCollection) || PayloadOf(handle) >= entries.size()) return nullptr;
    return &entries[PayloadOf(handle)];
  }
};
}  // namespace openjoey::lacooda64::runtime
