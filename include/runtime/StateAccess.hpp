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
  // Keep instance identity stable. Resolve the instance even if source coordinates
  // are old; honor destination and return its actual new location. A request
  // for the current location must succeed without losing the instance.
  virtual bool Relocate(Address object, Address destination, Address& moved) = 0;
  // Allocate from a numeric prototype; failure must not create an object.
  // These are storage primitives. The runtime executes all operation semantics.
  virtual bool Create(Word prototype, Address destination, Address& created);
  // Exchange occupied locations atomically while preserving instance identities.
  virtual bool Exchange(Address first, Address second, Address& first_moved, Address& second_moved);
};
}  // namespace openjoey::lacooda64::runtime
