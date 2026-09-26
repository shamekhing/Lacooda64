#pragma once
#include <array>

#include "lacooda/Address.hpp"
namespace openjoey::lacooda64::runtime {
// Numeric storage bindings, never card-specific ISA constants. Zero is unbound.
struct StateLayout {
  AttributeId position{}, face_up{}, defense_position{};
  AttributeId controller{}, owner{}, equip_target{};
  AttributeId last_summoned{};           // Optional player attribute containing an address.
  std::array<AttributeId, 4> negated{};  // Indexed by NegateOp.
  SignedWord face_up_attack = 1, face_up_defense = 2;
  SignedWord face_down_defense = 3, face_down_attack = 4;
};
}  // namespace openjoey::lacooda64::runtime
