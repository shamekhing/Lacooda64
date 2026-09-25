#pragma once

#include <vector>

#include "lacooda/Address.hpp"

namespace openjoey::lacooda64::runtime {
// Storage access only. Instructions, branches, queries and random generation
// execute in Lacooda; a storage implementation does not interpret effects.
class StateAccess {
 public:
  virtual ~StateAccess() = default;
  virtual bool Read(Address attribute, Word& value) const = 0;
  virtual bool Write(Address attribute, Word value) = 0;
  virtual bool Members(Address container, std::vector<Address>& result) const = 0;
  // Keep instance identity stable. Return the object's new location.
  virtual bool Relocate(Address object, Address destination, Address& moved) = 0;
};
}  // namespace openjoey::lacooda64::runtime
