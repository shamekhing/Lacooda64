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
//   59........53 52..50 49..48 47..42 41..36 35........12 11........0
//   +-----------+------+-----+------+------+-------------+------------+
//   | reserved  |level |  p  | zone | slot | card inst.  | attribute  |
//   +-----------+------+-----+------+------+-------------+------------+
//       7          3      2      6      6        24            12
//
// Full 64-bit word:
//   63..60 = WordTag::Address
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
    Duel = 0,  // Whole-duel scope.
    Player,    // One player's scope.
    Zone,      // A named zone within a player.
    Slot,      // A slot within a zone (e.g. a monster slot).
    Card,      // A specific card instance within a slot.
};

// Field widths (in payload bits) and derived shifts/masks for packing an address.
inline constexpr Word ATTRIBUTE_BITS = 12;
inline constexpr Word CARD_BITS = 24;
inline constexpr Word SLOT_BITS = 6;
inline constexpr Word ZONE_BITS = 6;
inline constexpr Word PLAYER_BITS = 2;
inline constexpr Word LEVEL_BITS = 3;

inline constexpr Word ATTRIBUTE_SHIFT = 0;
inline constexpr Word CARD_SHIFT = ATTRIBUTE_SHIFT + ATTRIBUTE_BITS; // 12
inline constexpr Word SLOT_SHIFT = CARD_SHIFT + CARD_BITS;           // 36
inline constexpr Word ZONE_SHIFT = SLOT_SHIFT + ZONE_BITS;           // 42
inline constexpr Word PLAYER_SHIFT = ZONE_SHIFT + ZONE_BITS;         // 48
inline constexpr Word LEVEL_SHIFT = PLAYER_SHIFT + PLAYER_BITS;      // 50

inline constexpr Word ATTRIBUTE_MASK = (Word{1} << ATTRIBUTE_BITS) - 1;
inline constexpr Word CARD_MASK = (Word{1} << CARD_BITS) - 1;
inline constexpr Word SLOT_MASK = (Word{1} << SLOT_BITS) - 1;
inline constexpr Word ZONE_MASK = (Word{1} << ZONE_BITS) - 1;
inline constexpr Word PLAYER_MASK = (Word{1} << PLAYER_BITS) - 1;
inline constexpr Word LEVEL_MASK = (Word{1} << LEVEL_BITS) - 1;

// Attribute selector meaning "the object itself" (no attribute).
inline constexpr AttributeId Self = 0;

// Builds an Address-tagged operand from a level and optional player/zone/slot/
// card/attribute components. Unspecified components default to 0.
[[nodiscard]] constexpr Address makeAddress(
    AddressLevel level,
    PlayerId player = 0,
    ZoneId zone = 0,
    SlotId slot = 0,
    CardInstanceId card = 0,
    AttributeId attribute = Self
) noexcept {
    const Word p =
        ((static_cast<Word>(level) & LEVEL_MASK) << LEVEL_SHIFT) |
        ((player & PLAYER_MASK) << PLAYER_SHIFT) |
        ((zone & ZONE_MASK) << ZONE_SHIFT) |
        ((slot & SLOT_MASK) << SLOT_SHIFT) |
        ((card & CARD_MASK) << CARD_SHIFT) |
        ((attribute & ATTRIBUTE_MASK) << ATTRIBUTE_SHIFT);
    return makeTagged(WordTag::Address, p);
}

// Whole-duel address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Duel(AttributeId a = Self) noexcept {
    return makeAddress(AddressLevel::Duel, 0, 0, 0, 0, a);
}
// Player-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Player(PlayerId p, AttributeId a = Self) noexcept {
    return makeAddress(AddressLevel::Player, p, 0, 0, 0, a);
}
// Zone-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Zone(PlayerId p, ZoneId z, AttributeId a = Self) noexcept {
    return makeAddress(AddressLevel::Zone, p, z, 0, 0, a);
}
// Slot-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Slot(PlayerId p, ZoneId z, SlotId s,
                                     AttributeId a = Self) noexcept {
    return makeAddress(AddressLevel::Slot, p, z, s, 0, a);
}
// Card-scope address (optionally targeting an attribute).
[[nodiscard]] constexpr Address Card(PlayerId p, ZoneId z, SlotId s,
                                     CardInstanceId c,
                                     AttributeId a = Self) noexcept {
    return makeAddress(AddressLevel::Card, p, z, s, c, a);
}

// --- accessors: decode each packed field from an address's payload.

// Returns the address level (depth) of `a`.
[[nodiscard]] constexpr AddressLevel levelOf(Address a) noexcept {
    return static_cast<AddressLevel>((payloadOf(a) >> LEVEL_SHIFT) & LEVEL_MASK);
}
// Returns the player id encoded in `a`.
[[nodiscard]] constexpr PlayerId playerOf(Address a) noexcept {
    return (payloadOf(a) >> PLAYER_SHIFT) & PLAYER_MASK;
}
// Returns the zone id encoded in `a`.
[[nodiscard]] constexpr ZoneId zoneOf(Address a) noexcept {
    return (payloadOf(a) >> ZONE_SHIFT) & ZONE_MASK;
}
// Returns the slot id encoded in `a`.
[[nodiscard]] constexpr SlotId slotOf(Address a) noexcept {
    return (payloadOf(a) >> SLOT_SHIFT) & SLOT_MASK;
}
// Returns the card-instance id encoded in `a`.
[[nodiscard]] constexpr CardInstanceId cardOf(Address a) noexcept {
    return (payloadOf(a) >> CARD_SHIFT) & CARD_MASK;
}
// Returns the attribute id encoded in `a`.
[[nodiscard]] constexpr AttributeId attributeOf(Address a) noexcept {
    return (payloadOf(a) >> ATTRIBUTE_SHIFT) & ATTRIBUTE_MASK;
}

// Returns a copy of `a` with its attribute field replaced by `attr`.
[[nodiscard]] constexpr Address withAttribute(Address a, AttributeId attr) noexcept {
    Word p = payloadOf(a);
    p &= ~(ATTRIBUTE_MASK << ATTRIBUTE_SHIFT);
    p |= (attr & ATTRIBUTE_MASK) << ATTRIBUTE_SHIFT;
    return makeTagged(WordTag::Address, p);
}

// --- predicates.

// True when `a` is Address-tagged.
[[nodiscard]] constexpr bool isAddress(Address a) noexcept {
    return isTag(a, WordTag::Address);
}
// True when `a` points at a whole object (attribute == Self).
[[nodiscard]] constexpr bool isObject(Address a) noexcept {
    return isAddress(a) && attributeOf(a) == Self;
}
// True when `a` points at a specific attribute of an object.
[[nodiscard]] constexpr bool isAttribute(Address a) noexcept {
    return isAddress(a) && attributeOf(a) != Self;
}
// True when `a` is Address-tagged and its level is within the supported range.
[[nodiscard]] constexpr bool validAddress(Address a) noexcept {
    return isAddress(a) &&
           static_cast<Word>(levelOf(a)) <= static_cast<Word>(AddressLevel::Card);
}

} // namespace openjoey::lacooda64
