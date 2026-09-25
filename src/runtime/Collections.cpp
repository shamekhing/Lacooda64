#include "runtime/Collections.hpp"

#include <utility>

namespace openjoey::lacooda64::runtime {
Word Collections::Add(std::vector<Address> values) {
  entries.push_back(std::move(values));
  return MakeTagged(WordTag::kCollection, entries.size() - 1);
}
std::vector<Address>* Collections::Get(Word handle) {
  if (!IsTag(handle, WordTag::kCollection) || PayloadOf(handle) >= entries.size()) return nullptr;
  return &entries[PayloadOf(handle)];
}
}  // namespace openjoey::lacooda64::runtime
