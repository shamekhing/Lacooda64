#include "Disassembler.hpp"

#include <sstream>

#include "Assembler.hpp"
#include "lacooda/Builder.hpp"

namespace openjoey::lacooda64::assembly {
namespace {
struct Spelling {
  std::string_view type, name;
  Word value;
};
constexpr Spelling spellings[]{
#define LACOODA_SPELLING(type, name, text) {#type, text, Sub(type::name)},
#include "Spellings.def"
#undef LACOODA_SPELLING
};
std::string Name(std::string_view type, Word value) {
  for (const auto& e : spellings)
    if (e.type == type && e.value == value) return std::string(e.name);
  return "?";
}
std::string OperandText(Operand operand) {
  if (IsNone(operand)) return "NONE";
  if (IsImmediate(operand)) return "#" + std::to_string(ImmediateValue(operand));
  if (IsRegister(operand)) return std::string(IsValueRegister(operand) ? "V" : IsAddressRegister(operand) ? "A" : "F") + std::to_string(RegisterIndexOf(operand));
  if (IsControl(operand)) {
    const char* names[] = {"TURN", "PHASE", "STEP", "CHAIN", "EFFECT", "PC", "RESULT_COUNT", "RESULT_SUCCESS", "EVENT_KIND", "EVENT_SUBJECT", "EVENT_OBJECT", "EVENT_CONTEXT", "PENDING_DESTINATION", "PENDING_SOURCE0", "PENDING_SOURCE1", "PENDING_CANCELLED"};
    const auto index = Sub(ControlRegOf(operand));
    if (index < std::size(names)) return std::string("CONTROL.") + names[index];
  }
  if (IsAddress(operand)) {
    const auto a = DecodeAddress(operand);
    std::string text = "[";
    if (a.level >= AddressLevel::kPlayer) text += std::to_string(a.player) + ":";
    if (a.level >= AddressLevel::kZone) text += std::to_string(a.zone) + ":";
    if (a.level >= AddressLevel::kSlot) text += std::to_string(a.slot) + ":";
    if (a.level >= AddressLevel::kCard) text += std::to_string(a.card) + ":";
    return text + std::to_string(a.attribute) + "]";
  }
  return "?";
}
std::string Words(const Instruction& i) {
  std::ostringstream out;
  out << "WORDS ";
  for (std::size_t n = 0; n < 4; ++n) {
    if (n) out << ", ";
    out << "0x" << std::hex << i[n];
  }
  return out.str();
}
std::string InstructionText(const Instruction& i) {
  auto op = OpcodeOf(Operation(i));
  auto sub = SubcodeOf(Operation(i));
  const auto operation = Operation(i);
  if (operation != Op(op, sub, FlagsOf(operation), CauseOf(operation), AuxOf(operation))) return Words(i);
  const auto d = OperandText(Dst(i)), s = OperandText(Src0(i)), t = OperandText(Src1(i));
  std::string args;
  switch (op) {
    case Opcode::kNop:
    case Opcode::kHalt:
      break;
    case Opcode::kJump:
      args = std::to_string(ImmediateValue(Src0(i)));
      break;
    case Opcode::kJumpIf:
      args = s + ", " + std::to_string(ImmediateValue(Src1(i))) + ", " + Name("JumpCondition", sub);
      break;
    case Opcode::kCompare:
      args = d + ", " + s + ", " + t + ", " + Name("CompareOp", sub);
      break;
    case Opcode::kAlu:
      args = d + ", " + s + ", " + (IsNone(Src1(i)) ? "" : t + ", ") + Name("AluOp", sub);
      break;
    case Opcode::kMove:
      args = Name("MoveMethod", sub) + ", " + d + ", " + s;
      if (!IsNone(Src1(i))) args += ", " + t;
      break;
    case Opcode::kSummon:
      args = Name("SummonMethod", Sub(SummonMethodOf(sub))) + ", " + Name("SummonMode", Sub(SummonModeOf(sub))) + ", " + d + ", " + s;
      break;
    case Opcode::kPosition:
    case Opcode::kNegate:
      args = Name(op == Opcode::kPosition ? "PositionOp" : "NegateOp", sub) + ", " + d;
      break;
    case Opcode::kEquip:
    case Opcode::kCounter:
    case Opcode::kControl:
    case Opcode::kRestrict:
      args = Name(op == Opcode::kEquip ? "EquipOp" : op == Opcode::kCounter ? "CounterOp" : op == Opcode::kControl ? "ControlOp" : "RestrictOp", sub) + ", " + d + ", " + s;
      break;
    case Opcode::kRandom:
      args = Name("RandomKind", sub) + ", " + d + ", " + s;
      if (!IsNone(Src1(i))) args += ", " + t;
      break;
    case Opcode::kEvent:
      args = Name("EventKind", sub) + ", " + d + ", " + s + ", " + t;
      break;
    case Opcode::kChain:
      args = Name("ChainOp", sub) + ", " + d + ", " + s;
      break;
    case Opcode::kAt:
    case Opcode::kAttribute:
    case Opcode::kChoose:
      args = d + ", " + s + ", " + t;
      break;
    case Opcode::kStage:
    case Opcode::kSchedule:
    case Opcode::kSubscribe:
      args = d + ", " + std::to_string(ImmediateValue(Src0(i))) + ", " + t;
      break;
    case Opcode::kCommit:
    case Opcode::kCancel:
    case Opcode::kUnmodify:
      args = s;
      break;
    case Opcode::kHistory:
      args = d + ", " + Name("HistoryField", sub);
      if (!IsNone(Src0(i))) args += ", " + s;
      break;
    case Opcode::kModify:
      args = d + ", " + s + ", " + t + ", " + Name("ModifierOp", sub);
      break;
    default:
      args = d + ", " + s;
      break;
  }
  auto text = Name("Opcode", Sub(op)) + (args.empty() ? "" : " " + args);
  if (FlagsOf(operation) || Sub(CauseOf(operation)) || AuxOf(operation))
    text += " | flags=" + std::to_string(FlagsOf(operation)) + " cause=" + std::to_string(Sub(CauseOf(operation))) + " aux=" + std::to_string(AuxOf(operation));
  return text;
}
}  // namespace
std::string Disassemble(const Trace& trace) {
  std::ostringstream out;
  out << "PC Instruction Meaning\n";
  for (std::size_t pc = 0; pc < trace.size(); ++pc) out << pc << ' ' << InstructionText(trace[pc]) << '\n';
  auto text = out.str();
  auto restored = Assemble(text);
  if (restored.ok() && restored.trace == trace) return text;
  // Preserve noncanonical/reserved bits and unused slots, never normalize them.
  out.str({});
  out.clear();
  out << "PC Instruction Meaning\n";
  for (std::size_t pc = 0; pc < trace.size(); ++pc) out << pc << ' ' << Words(trace[pc]) << '\n';
  return out.str();
}
}  // namespace openjoey::lacooda64::assembly
