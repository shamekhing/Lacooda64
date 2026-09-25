#pragma once
#include <map>

#include "StateAccess.hpp"
#include "lacooda/Opcode.hpp"
namespace openjoey::lacooda64::runtime {
// Base values remain in StateAccess. Removing a modifier removes only that
// contribution; it does not restore a stale saved value over later changes.
class Modifiers {
 public:
  Word Add(Address field, ModifierOp operation, SignedWord value);
  bool Remove(Word id);
  bool Read(const StateAccess& state, Address field, Word& value) const;

 private:
  struct Entry {
    Address field;
    ModifierOp operation;
    SignedWord value;
  };
  std::map<Word, Entry> entries_;
  Word next_ = 1;
};
class ModifiedState final : public StateAccess {
 public:
  ModifiedState(StateAccess& base, const Modifiers& modifiers);
  bool Read(Address field, Word& value) const override;
  bool Write(Address field, Word value) override;
  bool Members(Address container, std::vector<Address>& result) const override;
  bool Relocate(Address object, Address destination, Address& moved) override;

 private:
  StateAccess& base_;
  const Modifiers& modifiers_;
};
}  // namespace openjoey::lacooda64::runtime
