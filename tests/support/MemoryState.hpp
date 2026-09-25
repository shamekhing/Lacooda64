#pragma once
#include <algorithm>
#include <map>

#include "runtime/StateAccess.hpp"
namespace test {
using namespace openjoey::lacooda64;
// Test fixture only. Production code can expose its existing storage directly.
struct MemoryState : runtime::StateAccess {
  std::map<Address, Word> fields;
  std::map<Address, std::vector<Address>> zones;
  bool Read(Address a, Word& v) const override {
    auto it = fields.find(a);
    if (it == fields.end()) return false;
    v = it->second;
    return true;
  }
  bool Write(Address a, Word v) override {
    if (!IsAttribute(a)) return false;
    fields[a] = v;
    return true;
  }
  bool Members(Address a, std::vector<Address>& values) const override {
    auto it = zones.find(a);
    if (it == zones.end()) return false;
    values = it->second;
    return true;
  }
  bool Relocate(Address object, Address destination, Address& moved) override {
    auto target = zones.find(destination);
    if (target == zones.end()) return false;
    for (auto& [zone, values] : zones) {
      auto it = std::find(values.begin(), values.end(), object);
      if (it == values.end()) continue;
      if (zone == destination) return false;
      moved =
          Card(PlayerOf(destination), ZoneOf(destination), 0, CardOf(object));
      values.erase(it);
      target->second.push_back(moved);
      return true;
    }
    return false;
  }
};
}  // namespace test
