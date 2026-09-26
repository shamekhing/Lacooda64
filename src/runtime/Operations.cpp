#include <algorithm>

#include "Numeric.hpp"
#include "runtime/Runtime.hpp"

namespace openjoey::lacooda64::runtime {
namespace {
bool Field(AttributeId field) { return field > 0 && field <= kAttributeMask; }
bool CardObject(Address address) { return ValidAddress(address) && IsObject(address) && LevelOf(address) == AddressLevel::kCard; }
bool SameCard(Address a, Address b) { return CardObject(a) && CardObject(b) && CardOf(a) == CardOf(b); }
bool PositionLayout(const StateLayout& layout) {
  if (!Field(layout.position) || !Field(layout.face_up) || !Field(layout.defense_position) || layout.position == layout.face_up || layout.position == layout.defense_position || layout.face_up == layout.defense_position) return false;
  const SignedWord values[]{layout.face_up_attack, layout.face_up_defense, layout.face_down_defense, layout.face_down_attack};
  for (std::size_t i = 0; i < 4; ++i) {
    if (values[i] < kImmediateMin || values[i] > kImmediateMax) return false;
    for (std::size_t j = 0; j < i; ++j)
      if (values[i] == values[j]) return false;
  }
  return true;
}
bool ReadNumber(StateAccess& state, Address field, SignedWord& result) {
  Word value;
  if (!state.Read(field, value) || !IsImmediate(value)) return false;
  result = ImmediateValue(value);
  return true;
}
}  // namespace

bool Runtime::ApplyPosition(StateAccess& state, Address object, PositionOp operation) {
  if (!CardObject(object) || !PositionLayout(layout_)) return false;
  bool up = true, defense = false;
  if (operation == PositionOp::kFaceUpAttack) {
  } else if (operation == PositionOp::kFaceUpDefense)
    defense = true;
  else if (operation == PositionOp::kFaceDownDefense) {
    up = false;
    defense = true;
  } else {
    SignedWord position;
    if (!ReadNumber(state, WithAttribute(object, layout_.position), position)) return false;
    if (position == layout_.face_up_attack) {
    } else if (position == layout_.face_up_defense)
      defense = true;
    else if (position == layout_.face_down_defense) {
      up = false;
      defense = true;
    } else if (position == layout_.face_down_attack)
      up = false;
    else
      return false;
    switch (operation) {
      case PositionOp::kAttack:
        defense = false;
        break;
      case PositionOp::kDefense:
        defense = true;
        break;
      case PositionOp::kFaceUp:
        up = true;
        break;
      case PositionOp::kFaceDown:
        up = false;
        break;
      case PositionOp::kToggle:
        defense = !defense;
        break;
      default:
        return false;
    }
  }
  const auto position = up ? (defense ? layout_.face_up_defense : layout_.face_up_attack) : (defense ? layout_.face_down_defense : layout_.face_down_attack);
  return state.Write(WithAttribute(object, layout_.position), Imm(position)) && state.Write(WithAttribute(object, layout_.face_up), Imm(up)) && state.Write(WithAttribute(object, layout_.defense_position), Imm(defense));
}

bool Runtime::ApplyStateOperation(Frame& frame, const Instruction& instruction) {
  ModifiedState state(state_, modifiers);
  const auto operation = Operation(instruction);
  const auto opcode = OpcodeOf(operation);
  const auto sub = SubcodeOf(operation);
  const auto d = Dst(instruction), s = Src0(instruction), t = Src1(instruction);
  auto address = [&](Operand operand, Address& result) { return ResolveAddress(frame, operand, result); };
  auto number = [&](Operand operand, SignedWord& result) {
    Word value;
    if (!ReadOperand(frame, state, operand, value) || !IsImmediate(value)) return false;
    result = ImmediateValue(value);
    return true;
  };
  auto event = [&](EventKind kind, Word subject, Word object = kNone) { Deliver({Sub(kind), subject, object, operation}); };
  auto count = [&](SignedWord value) { frame.machine.control[ControlIndex(ControlReg::kResultCount)] = Imm(value); };
  auto relocate = [&](Address object, Address destination, Address& moved) {
    if (!state.Relocate(object, destination, moved) || !CardObject(moved) || !SameCard(object, moved)) return false;
    return true;
  };
  Address dst{}, src{};
  if (!address(d, dst)) return false;

  switch (opcode) {
    case Opcode::kPosition:
      if (!ApplyPosition(state, dst, static_cast<PositionOp>(sub))) return false;
      count(1);
      event(EventKind::kPositionChanged, dst);
      return true;
    case Opcode::kSummon: {
      if (!IsObject(dst) || (LevelOf(dst) != AddressLevel::kSlot && LevelOf(dst) != AddressLevel::kCard) || !PositionLayout(layout_) || !Field(layout_.controller) || (layout_.last_summoned && !Field(layout_.last_summoned))) return false;
      const auto method = SummonMethodOf(sub);
      const auto mode = SummonModeOf(sub);
      const auto position = mode == SummonMode::kFaceUpDefense ? PositionOp::kFaceUpDefense : mode == SummonMode::kFaceDownDefense || mode == SummonMode::kSet ? PositionOp::kFaceDownDefense : PositionOp::kFaceUpAttack;
      Address moved{};
      if (method == SummonMethod::kToken) {
        SignedWord prototype;
        if (!Field(layout_.owner) || !number(s, prototype) || prototype < 0 || !state.Create(Word(prototype), dst, moved) || !CardObject(moved)) return false;
        if (!state.Write(WithAttribute(moved, layout_.owner), Imm(SignedWord(PlayerOf(dst))))) return false;
      } else {
        if (!address(s, src) || !CardObject(src)) return false;
        if (method == SummonMethod::kFlip) {
          SignedWord up;
          if (PlayerOf(src) != PlayerOf(dst) || ZoneOf(src) != ZoneOf(dst) || SlotOf(src) != SlotOf(dst) || !ReadNumber(state, WithAttribute(src, layout_.face_up), up) || up != 0 || (mode != SummonMode::kDefault && mode != SummonMode::kFaceUpAttack)) return false;
          moved = src;
        } else if (!relocate(src, dst, moved))
          return false;
      }
      if (PlayerOf(moved) != PlayerOf(dst) || ZoneOf(moved) != ZoneOf(dst) || SlotOf(moved) != SlotOf(dst)) return false;
      if (!ApplyPosition(state, moved, position) || !state.Write(WithAttribute(moved, layout_.controller), Imm(SignedWord(PlayerOf(moved))))) return false;
      if (layout_.last_summoned && !state.Write(Player(PlayerOf(moved), layout_.last_summoned), moved)) return false;
      if (IsAddressRegister(s) && !WriteOperand(frame, state, s, moved)) return false;
      count(1);
      event(EventKind::kSummoned, moved, src);
      return true;
    }
    case Opcode::kEquip: {
      if (!CardObject(dst) || !address(s, src) || !CardObject(src) || SameCard(dst, src) || !Field(layout_.equip_target)) return false;
      Word previous;
      if (!state.Read(WithAttribute(src, layout_.equip_target), previous)) return false;
      const bool attached = CardObject(previous);
      if (!attached && previous != Imm(0) && previous != kNone) return false;
      const auto method = static_cast<EquipOp>(sub);
      if ((method == EquipOp::kAttach && attached) || (method != EquipOp::kAttach && !attached) || (method == EquipOp::kDetach && !SameCard(previous, dst))) return false;
      if (!state.Write(WithAttribute(src, layout_.equip_target), method == EquipOp::kDetach ? Imm(0) : dst)) return false;
      count(1);
      if (attached) event(EventKind::kUnequipped, src, previous);
      if (method != EquipOp::kDetach) event(EventKind::kEquipped, src, dst);
      return true;
    }
    case Opcode::kCounter: {
      if (IsObject(dst)) {
        if (!CardObject(dst) || !Field(AuxOf(operation))) return false;
        dst = WithAttribute(dst, AuxOf(operation));
      }
      if (!IsAttribute(dst)) return false;
      SignedWord old, amount, updated;
      if (!ReadNumber(state, dst, old) || old < 0) return false;
      const auto method = static_cast<CounterOp>(sub);
      if (method == CounterOp::kTransfer) {
        if (!address(s, src)) return false;
        if (IsObject(src)) {
          if (!CardObject(src) || !Field(AuxOf(operation))) return false;
          src = WithAttribute(src, AuxOf(operation));
        }
        SignedWord available;
        if (!IsAttribute(src) || !ReadNumber(state, src, available) || available < 0) return false;
        amount = available;
        if (!IsNone(t) && !number(t, amount)) return false;
        if (amount < 0 || amount > available) return false;
        // Identity aliases refer to the same counter even after relocation.
        const bool same = src == dst || (LevelOf(src) == AddressLevel::kCard && LevelOf(dst) == AddressLevel::kCard && CardOf(src) == CardOf(dst) && AttributeOf(src) == AttributeOf(dst));
        if (same)
          amount = 0;
        else {
          if (!Arithmetic(AluOp::kAdd, old, amount, updated)) return false;
          if (!state.Write(src, Imm(available - amount)) || !state.Write(dst, Imm(updated))) return false;
        }
      } else {
        if (!number(s, amount) || amount < 0) return false;
        if (method == CounterOp::kSet)
          updated = amount;
        else if (method == CounterOp::kRemove) {
          if (amount > old) return false;
          updated = old - amount;
        } else if (!Arithmetic(AluOp::kAdd, old, amount, updated))
          return false;
        if (!state.Write(dst, Imm(updated))) return false;
      }
      count(amount);
      event(EventKind::kCounterChanged, dst, src);
      return true;
    }
    case Opcode::kRestrict: {
      SignedWord amount, old, updated;
      if (!IsAttribute(dst) || !number(s, amount) || amount < 0) return false;
      switch (static_cast<RestrictOp>(sub)) {
        case RestrictOp::kApply:
          updated = amount;
          break;
        case RestrictOp::kClear:
          updated = 0;
          break;
        case RestrictOp::kIncrement:
        case RestrictOp::kDecrement:
          if (!ReadNumber(state, dst, old) || old < 0) return false;
          if (sub == Sub(RestrictOp::kDecrement)) {
            if (amount > old) return false;
            updated = old - amount;
          } else if (!Arithmetic(AluOp::kAdd, old, amount, updated))
            return false;
          break;
        default:
          return false;
      }
      if (!state.Write(dst, Imm(updated))) return false;
      count(1);
      return true;
    }
    case Opcode::kControl: {
      if (!IsObject(dst) || !address(s, src) || !CardObject(src) || !Field(layout_.controller)) return false;
      const auto method = static_cast<ControlOp>(sub);
      Address moved{}, other_moved{};
      if (method == ControlOp::kSwap) {
        if (!CardObject(dst) || SameCard(dst, src) || !state.Exchange(src, dst, moved, other_moved) || !SameCard(src, moved) || !SameCard(dst, other_moved) || PlayerOf(moved) != PlayerOf(dst) || ZoneOf(moved) != ZoneOf(dst) || SlotOf(moved) != SlotOf(dst) || PlayerOf(other_moved) != PlayerOf(src) || ZoneOf(other_moved) != ZoneOf(src) || SlotOf(other_moved) != SlotOf(src)) return false;
        if (!state.Write(WithAttribute(moved, layout_.controller), Imm(SignedWord(PlayerOf(moved)))) || !state.Write(WithAttribute(other_moved, layout_.controller), Imm(SignedWord(PlayerOf(other_moved))))) return false;
        if (IsAddressRegister(d) && !WriteOperand(frame, state, d, other_moved)) return false;
        event(EventKind::kControlChanged, other_moved, dst);
        count(2);
      } else {
        if (LevelOf(dst) == AddressLevel::kPlayer) dst = Zone(PlayerOf(dst), ZoneOf(src));
        if (LevelOf(dst) != AddressLevel::kZone && LevelOf(dst) != AddressLevel::kSlot) return false;
        if (method == ControlOp::kReturn) {
          SignedWord owner;
          if (!Field(layout_.owner) || !ReadNumber(state, WithAttribute(src, layout_.owner), owner) || owner < 0 || Word(owner) != PlayerOf(dst)) return false;
        }
        // Let storage resolve the identity; encoded source coordinates may be old.
        if (!relocate(src, dst, moved)) return false;
        if (PlayerOf(moved) != PlayerOf(dst) || ZoneOf(moved) != ZoneOf(dst) || (LevelOf(dst) == AddressLevel::kSlot && SlotOf(moved) != SlotOf(dst))) return false;
        if (!state.Write(WithAttribute(moved, layout_.controller), Imm(SignedWord(PlayerOf(moved))))) return false;
        count(1);
      }
      if (IsAddressRegister(s) && !WriteOperand(frame, state, s, moved)) return false;
      event(EventKind::kControlChanged, moved, src);
      return true;
    }
    case Opcode::kNegate: {
      const auto method = static_cast<NegateOp>(sub);
      if (sub >= layout_.negated.size()) return false;
      bool matched = false;
      if (method == NegateOp::kActivation || method == NegateOp::kEffect) {
        for (auto it = chain.links.rbegin(); it != chain.links.rend(); ++it) {
          if (!it->resolved && (it->subject == dst || SameCard(it->subject, dst))) {
            it->negated = true;
            matched = true;
            break;
          }
        }
      }
      if (frame.pending && !frame.pending->completed) {
        auto& pending = *frame.pending->operation;
        const auto action = OpcodeOf(Operation(pending.instruction));
        if (method == NegateOp::kSummon && action == Opcode::kSummon && SameCard(Src0(pending.instruction), dst)) {
          pending.cancelled = Imm(1);
          matched = true;
        }
      }
      if (IsAttribute(dst)) {
        if (!state.Write(dst, Imm(1))) return false;
        matched = true;
      } else if (Field(layout_.negated[sub])) {
        if (!state.Write(WithAttribute(dst, layout_.negated[sub]), Imm(1))) return false;
        matched = true;
      }
      if (!matched) return false;
      count(1);
      event(method == NegateOp::kActivation ? EventKind::kActivationNegated : method == NegateOp::kAttack ? EventKind::kAttackCanceled : method == NegateOp::kSummon ? EventKind::kReplacement : EventKind::kEffectNegated, dst);
      return true;
    }
    default:
      return false;
  }
}
}  // namespace openjoey::lacooda64::runtime
