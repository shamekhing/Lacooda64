#pragma once
// OpenJoey2 - Lacooda / 64-bit word VM.
// Address word: packed [level|player|zone|slot|card|attribute].

#include "Word.hpp"

namespace openjoey::lacooda64 {

// -----------------------------------------------------------------------------
// Address word
//
// Payload bits (53 used, 7 reserved):
//
//   payload bit
//   59..53 52..50 49..48 47..42 41..36 35..12 11..0
//   +-------+------+-----+------+------+---------+----------+
//   | rsvd | level |  p  | zone | slot | card inst | attribute|
//   +-------+------+-----+------+------+---------+----------+
//       7      3      2      6      6        24         12
//
// Full 64-bit word:
//   63..60 = WordTag::kAddress
//
// Address depth:
//   Duel   [a]
//   Player [p:a]
//   Zone   [p:z:a]
//   Slot   [p:z:sl:a]
//   Card   [p:z:sl:c:a]
// -----------------------------------------------------------------------------

// How deep into the duel location hierarchy the address resolves.
enum class AddressLevel : Word {
  kDuel = 0,  // Whole-duel scope.
  kPlayer,    // One player's scope.
  kZone,      // A named zone within a player.
  kSlot,      // A slot within a zone (e.g. a monster slot).
  kCard,      // A specific card instance within a slot.
};

// Field widths (in payload bits) and derived shifts/masks for packing an
// address.
inline constexpr Word kAttributeBits = 12;
inline constexpr Word kCardBits = 24;
inline constexpr Word kSlotBits = 6;
inline constexpr Word kZoneBits = 6;
inline constexpr Word kPlayerBits = 2;
inline constexpr Word kLevelBits = 3;

inline constexpr Word kAttributeShift = 0;
inline constexpr Word kCardShift = kAttributeShift + kAttributeBits;
inline constexpr Word kSlotShift = kCardShift + kCardBits;
inline constexpr Word kZoneShift = kSlotShift + kSlotBits;
inline constexpr Word kPlayerShift = kZoneShift + kZoneBits;
inline constexpr Word kLevelShift = kPlayerShift + kPlayerBits;

inline constexpr Word kAttributeMask = (Word{1} << kAttributeBits) - 1;
inline constexpr Word kCardMask = (Word{1} << kCardBits) - 1;
inline constexpr Word kSlotMask = (Word{1} << kSlotBits) - 1;
inline constexpr Word kZoneMask = (Word{1} << kZoneBits) - 1;
inline constexpr Word kPlayerMask = (Word{1} << kPlayerBits) - 1;
inline constexpr Word kLevelMask = (Word{1} << kLevelBits) - 1;

// Attribute selector meaning "the object itself" (no attribute).
inline constexpr AttributeId kSelf = 0;

// Builds an Address-tagged operand from a level and optional player/zone/slot/
// card/attribute components. Unspecified components default to 0.
// Fields are masked to their widths; oversized input values are truncated.
[[nodiscard]] constexpr Address MakeAddress(
    AddressLevel level, PlayerId player = 0, ZoneId zone = 0, SlotId slot = 0,
    CardInstanceId card = 0, AttributeId attribute = kSelf) noexcept {
  const Word p = ((static_cast<Word>(level) & kLevelMask) << kLevelShift) |
                 ((player & kPlayerMask) << kPlayerShift) |
                 ((zone & kZoneMask) << kZoneShift) |
                 ((slot & kSlotMask) << kSlotShift) |
                 ((card & kCardMask) << kCardShift) |
                 ((attribute & kAttributeMask) << kAttributeShift);
  return MakeTagged(WordTag::kAddress, p);
}

// Whole-duel address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Duel(AttributeId a = kSelf) noexcept {
  return MakeAddress(AddressLevel::kDuel, 0, 0, 0, 0, a);
}
// Player-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Player(PlayerId p,
                                       AttributeId a = kSelf) noexcept {
  return MakeAddress(AddressLevel::kPlayer, p, 0, 0, 0, a);
}
// Zone-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Zone(PlayerId p, ZoneId z,
                                     AttributeId a = kSelf) noexcept {
  return MakeAddress(AddressLevel::kZone, p, z, 0, 0, a);
}
// Slot-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Slot(PlayerId p, ZoneId z, SlotId s,
                                     AttributeId a = kSelf) noexcept {
  return MakeAddress(AddressLevel::kSlot, p, z, s, 0, a);
}
// Card-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Card(PlayerId p, ZoneId z, SlotId s,
                                     CardInstanceId c,
                                     AttributeId a = kSelf) noexcept {
  return MakeAddress(AddressLevel::kCard, p, z, s, c, a);
}

// --- Accessors: decode each packed field from an address's payload.

// Returns the address level (depth) of `a`.
[[nodiscard]] constexpr AddressLevel LevelOf(Address a) noexcept {
  return static_cast<AddressLevel>((PayloadOf(a) >> kLevelShift) & kLevelMask);
}
// Returns the player id encoded in `a`.
[[nodiscard]] constexpr PlayerId PlayerOf(Address a) noexcept {
  return (PayloadOf(a) >> kPlayerShift) & kPlayerMask;
}
// Returns the zone id encoded in `a`.
[[nodiscard]] constexpr ZoneId ZoneOf(Address a) noexcept {
  return (PayloadOf(a) >> kZoneShift) & kZoneMask;
}
// Returns the slot id encoded in `a`.
[[nodiscard]] constexpr SlotId SlotOf(Address a) noexcept {
  return (PayloadOf(a) >> kSlotShift) & kSlotMask;
}
// Returns the card-instance id encoded in `a`.
[[nodiscard]] constexpr CardInstanceId CardOf(Address a) noexcept {
  return (PayloadOf(a) >> kCardShift) & kCardMask;
}
// Returns the attribute id encoded in `a`.
[[nodiscard]] constexpr AttributeId AttributeOf(Address a) noexcept {
  return (PayloadOf(a) >> kAttributeShift) & kAttributeMask;
}

// Returns a copy of `a` with its attribute field replaced by `attr`.
[[nodiscard]] constexpr Address WithAttribute(Address a,
                                              AttributeId attr) noexcept {
  Word p = PayloadOf(a);
  p &= ~(kAttributeMask << kAttributeShift);
  p |= (attr & kAttributeMask) << kAttributeShift;
  return MakeTagged(WordTag::kAddress, p);
}

// --- Predicates.

// True when `a` is Address-tagged.
[[nodiscard]] constexpr bool IsAddress(Address a) noexcept {
  return IsTag(a, WordTag::kAddress);
}
// True when `a` points at a whole object (attribute == kSelf).
[[nodiscard]] constexpr bool IsObject(Address a) noexcept {
  return IsAddress(a) && AttributeOf(a) == kSelf;
}
// True when `a` points at a specific attribute of an object.
[[nodiscard]] constexpr bool IsAttribute(Address a) noexcept {
  return IsAddress(a) && AttributeOf(a) != kSelf;
}
// True when `a` is Address-tagged and its level is within the supported range.
[[nodiscard]] constexpr bool ValidAddress(Address a) noexcept {
  return IsAddress(a) && static_cast<Word>(LevelOf(a)) <=
                             static_cast<Word>(AddressLevel::kCard);
}

}  // namespace openjoey::lacooda64
