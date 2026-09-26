#include "assembly/Assembler.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <sstream>
#include <stdexcept>
#include <string>

#include "lacooda/Builder.hpp"

namespace openjoey::lacooda64::assembly {
namespace {
std::string Trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == s.npos) return {};
  return std::string(s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1));
}
Word Unsigned(std::string_view s) {
  Word v{};
  int base = 10;
  if (s.starts_with("0x")) {
    s.remove_prefix(2);
    base = 16;
  }
  const auto r = std::from_chars(s.data(), s.data() + s.size(), v, base);
  if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size()) throw std::runtime_error("invalid unsigned integer");
  return v;
}
SignedWord Signed(std::string_view s) {
  SignedWord v{};
  const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
  if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size() || v < kImmediateMin || v > kImmediateMax) throw std::runtime_error("immediate out of range or malformed");
  return v;
}
std::vector<std::string> Split(std::string_view s, char separator = ',') {
  std::vector<std::string> out;
  std::size_t start = 0;
  int depth = 0;
  for (std::size_t n = 0; n < s.size(); ++n) {
    if (s[n] == '[') ++depth;
    if (s[n] == ']' && --depth < 0) throw std::runtime_error("unmatched closing bracket");
    if (s[n] == separator && depth == 0) {
      out.push_back(Trim(s.substr(start, n - start)));
      start = n + 1;
    }
  }
  if (depth) throw std::runtime_error("unclosed bracket");
  if (start < s.size())
    out.push_back(Trim(s.substr(start)));
  else if (!s.empty())
    throw std::runtime_error("empty operand");
  return out;
}
struct Spelling {
  std::string_view type, name;
  Word value;
};
constexpr Spelling spellings[]{
#define LACOODA_SPELLING(type, name, text) {#type, text, Sub(type::name)},
#include "Spellings.def"
#undef LACOODA_SPELLING
};
Word Enum(std::string_view type, std::string_view name) {
  for (const auto& e : spellings)
    if (e.type == type && e.name == name) return e.value;
  throw std::runtime_error("unknown " + std::string(type) + " spelling: " + std::string(name));
}
Operand OperandOf(const std::string& s, const Symbols& symbols) {
  if (s == "NONE") return kNone;
  if (auto it = symbols.find(s); it != symbols.end()) return it->second;
  if (s.starts_with('#')) return Imm(Signed(std::string_view(s).substr(1)));
  if (s.size() > 1 && (s[0] == 'V' || s[0] == 'A' || s[0] == 'F') && std::isdigit(static_cast<unsigned char>(s[1]))) {
    Word n = Unsigned(std::string_view(s).substr(1));
    if (n > kRegisterIndexMask) throw std::runtime_error("register index out of range");
    return s[0] == 'V' ? V(n) : s[0] == 'A' ? A(n) : F(n);
  }
  if (s.starts_with("CONTROL.")) {
    const std::string_view names[] = {"TURN", "PHASE", "STEP", "CHAIN", "EFFECT", "PC", "RESULT_COUNT", "RESULT_SUCCESS", "EVENT_KIND", "EVENT_SUBJECT", "EVENT_OBJECT", "EVENT_CONTEXT", "PENDING_DESTINATION", "PENDING_SOURCE0", "PENDING_SOURCE1", "PENDING_CANCELLED"};
    for (Word n = 0; n < std::size(names); ++n)
      if (std::string_view(s).substr(8) == names[n]) return Control(static_cast<ControlReg>(n));
  }
  if (s.starts_with('[') && s.ends_with(']')) {
    const auto parts = Split(std::string_view(s).substr(1, s.size() - 2), ':');
    if (parts.empty() || parts.size() > 5) throw std::runtime_error("address needs 1 to 5 fields");
    Word v[5]{};
    for (std::size_t n = 0; n < parts.size(); ++n) v[n] = Unsigned(parts[n]);
    Word p = 0, z = 0, sl = 0, c = 0, a = v[parts.size() - 1];
    if (parts.size() > 1) p = v[0];
    if (parts.size() > 2) z = v[1];
    if (parts.size() > 3) sl = v[2];
    if (parts.size() > 4) c = v[3];
    if (p > kPlayerMask || z > kZoneMask || sl > kSlotMask || c > kCardMask || a > kAttributeMask) throw std::runtime_error("address field out of range");
    return MakeAddress(static_cast<AddressLevel>(parts.size() - 1), p, z, sl, c, a);
  }
  throw std::runtime_error("unresolved operand: " + s);
}
struct Line {
  std::size_t source;
  std::string text;
};
}  // namespace
AssemblyResult Assemble(std::string_view source, const Symbols& supplied) {
  AssemblyResult result;
  Symbols symbols = supplied;
  std::map<std::string, Word> labels;
  std::vector<Line> lines;
  std::istringstream input{std::string(source)};
  std::string line;
  std::size_t lineno = 0;
  while (std::getline(input, line)) {
    ++lineno;
    try {
      line = Trim(std::string_view(line).substr(0, line.find(';')));
      if (line.empty() || line.starts_with("PC ")) continue;
      if (line.starts_with(".address ") || line.starts_with(".symbol ")) {
        auto pos = line.find(' ');
        auto body = Trim(std::string_view(line).substr(pos + 1));
        pos = body.find_first_of(" \t");
        if (pos == body.npos) throw std::runtime_error("symbol requires a name and value");
        auto name = body.substr(0, pos);
        auto value = OperandOf(Trim(std::string_view(body).substr(pos + 1)), symbols);
        if (!symbols.emplace(name, value).second || labels.contains(name)) throw std::runtime_error("duplicate symbol");
        continue;
      }
      if (line[0] == '.')
        throw std::runtime_error(
            "unsupported directive; symbolic effect records must be compiled "
            "to instructions");
      if (line.ends_with(':')) {
        auto name = line.substr(0, line.size() - 1);
        if (name.empty() || symbols.contains(name) || !labels.emplace(name, lines.size()).second) throw std::runtime_error("duplicate or empty label");
        continue;
      }
      if (std::isdigit(static_cast<unsigned char>(line[0]))) {
        auto pos = line.find_first_of(" \t");
        if (pos == line.npos || Unsigned(line.substr(0, pos)) != lines.size()) throw std::runtime_error("PC is not contiguous");
        line = Trim(std::string_view(line).substr(pos + 1));
      }
      lines.push_back({lineno, line});
    } catch (const std::exception& e) {
      result.diagnostics.push_back({lineno, e.what()});
    }
  }
  auto target = [&](const std::string& s) {
    if (auto it = labels.find(s); it != labels.end()) return JumpTarget(it->second);
    return JumpTarget(Unsigned(s));
  };
  for (const auto& ln : lines) {
    try {
      const auto annotation = ln.text.find('|');
      auto text = Trim(std::string_view(ln.text).substr(0, annotation));
      Word flags = 0, cause = 0, aux = 0;
      if (annotation != ln.text.npos) {
        std::istringstream metadata(ln.text.substr(annotation + 1));
        std::map<std::string, bool> seen;
        std::string item;
        while (metadata >> item) {
          auto eq = item.find('=');
          if (eq == item.npos) throw std::runtime_error("metadata requires key=value");
          auto key = item.substr(0, eq);
          if (!seen.emplace(key, true).second) throw std::runtime_error("duplicate metadata");
          auto value = Unsigned(item.substr(eq + 1));
          if (key == "flags" && value <= kFlagsMask)
            flags = value;
          else if (key == "cause" && value <= Sub(CauseKind::kReplacement))
            cause = value;
          else if (key == "aux" && value <= kAuxMask)
            aux = value;
          else
            throw std::runtime_error("unknown or out-of-range metadata");
        }
        if (seen.empty()) throw std::runtime_error("empty metadata");
      }
      auto pos = text.find_first_of(" \t");
      auto name = text.substr(0, pos);
      auto args = pos == text.npos ? std::vector<std::string>{} : Split(Trim(std::string_view(text).substr(pos + 1)));
      Instruction inst{};
      auto arity = [&](std::size_t lo, std::size_t hi) {
        if (args.size() < lo || args.size() > hi) throw std::runtime_error("wrong operand count for " + name);
      };
      if (name == "WORDS") {
        if (annotation != ln.text.npos) throw std::runtime_error("WORDS already includes metadata");
        arity(4, 4);
        for (std::size_t n = 0; n < 4; ++n) inst[n] = Unsigned(args[n]);
      } else {
        const auto op = static_cast<Opcode>(Enum("Opcode", name));
        Word sub = 0;
        Operand d = kNone, s = kNone, t = kNone;
        auto operand = [&](std::size_t n) { return OperandOf(args.at(n), symbols); };
        switch (op) {
          case Opcode::kNop:
          case Opcode::kHalt:
            arity(0, 0);
            break;
          case Opcode::kJump:
            arity(1, 1);
            s = target(args[0]);
            break;
          case Opcode::kJumpIf:
            arity(2, 3);
            s = operand(0);
            t = target(args[1]);
            sub = args.size() == 3 ? Enum("JumpCondition", args[2]) : Sub(JumpCondition::kTrue);
            break;
          case Opcode::kCompare:
            arity(4, 4);
            d = operand(0);
            s = operand(1);
            t = operand(2);
            sub = Enum("CompareOp", args[3]);
            break;
          case Opcode::kAlu:
            arity(3, 4);
            d = operand(0);
            s = operand(1);
            if (args.size() == 4) t = operand(2);
            sub = Enum("AluOp", args.back());
            break;
          case Opcode::kMove:
            arity(3, 4);
            sub = Enum("MoveMethod", args[0]);
            d = operand(1);
            s = operand(2);
            if (args.size() == 4) t = operand(3);
            break;
          case Opcode::kSummon:
            arity(4, 4);
            sub = SummonSubcode(static_cast<SummonMethod>(Enum("SummonMethod", args[0])), static_cast<SummonMode>(Enum("SummonMode", args[1])));
            d = operand(2);
            s = operand(3);
            break;
          case Opcode::kPosition:
          case Opcode::kNegate:
            arity(2, 2);
            sub = Enum(op == Opcode::kPosition ? "PositionOp" : "NegateOp", args[0]);
            d = operand(1);
            break;
          case Opcode::kEquip:
          case Opcode::kControl:
          case Opcode::kRestrict:
            arity(3, 3);
            sub = Enum(op == Opcode::kEquip ? "EquipOp" : op == Opcode::kCounter ? "CounterOp" : op == Opcode::kControl ? "ControlOp" : "RestrictOp", args[0]);
            d = operand(1);
            s = operand(2);
            break;
          case Opcode::kCounter:
            arity(3, 4);
            sub = Enum("CounterOp", args[0]);
            if (args.size() == 4 && sub != Sub(CounterOp::kTransfer)) throw std::runtime_error("only COUNTER TRANSFER takes a source and amount");
            d = operand(1);
            s = operand(2);
            if (args.size() == 4) t = operand(3);
            break;
          case Opcode::kRandom:
            arity(3, 4);
            sub = Enum("RandomKind", args[0]);
            d = operand(1);
            s = operand(2);
            if (args.size() == 4) t = operand(3);
            break;
          case Opcode::kEvent:
            arity(1, 4);
            sub = Enum("EventKind", args[0]);
            if (args.size() > 1) d = operand(1);
            if (args.size() > 2) s = operand(2);
            if (args.size() > 3) t = operand(3);
            break;
          case Opcode::kChain:
            arity(1, 3);
            sub = Enum("ChainOp", args[0]);
            if (args.size() > 1) d = operand(1);
            if (args.size() > 2) s = sub == Sub(ChainOp::kPush) && labels.contains(args[2]) ? target(args[2]) : operand(2);
            break;
          case Opcode::kAt:
          case Opcode::kAttribute:
          case Opcode::kChoose:
            arity(3, 3);
            d = operand(0);
            s = operand(1);
            t = operand(2);
            break;
          case Opcode::kStage:
          case Opcode::kSchedule:
          case Opcode::kSubscribe:
            arity(3, 3);
            d = operand(0);
            s = target(args[1]);
            t = operand(2);
            break;
          case Opcode::kHistory:
            arity(2, 3);
            d = operand(0);
            sub = Enum("HistoryField", args[1]);
            if (args.size() == 3) s = operand(2);
            break;
          case Opcode::kModify:
            arity(4, 4);
            d = operand(0);
            s = operand(1);
            t = operand(2);
            sub = Enum("ModifierOp", args[3]);
            break;
          case Opcode::kUnmodify:
          case Opcode::kCommit:
          case Opcode::kCancel:
            arity(1, 1);
            s = operand(0);
            break;
          default:
            arity(2, 2);
            d = operand(0);
            s = operand(1);
            break;
        }
        inst = MakeInstruction(Op(op, sub, flags, static_cast<CauseKind>(cause), aux), d, s, t);
      }
      if (!ValidInstruction(inst)) throw std::runtime_error("invalid instruction operand types");
      result.trace.push_back(inst);
    } catch (const std::exception& e) {
      result.diagnostics.push_back({ln.source, e.what()});
    }
  }
  if (result.diagnostics.empty()) {
    auto valid = ValidateTrace(result.trace);
    if (!valid) result.diagnostics.push_back({lines.at(valid.instruction_).source, "branch or registration target out of range"});
  }
  if (!result.ok()) result.trace.clear();
  return result;
}
ModuleResult AssembleModule(std::string_view source, const Symbols& symbols) {
  ModuleResult result;
  std::istringstream input{std::string(source)};
  std::string line, name, body;
  std::map<std::string, bool> names;
  std::size_t lineno = 0, start = 0;
  bool active = false, module = false, plain = false;
  while (std::getline(input, line)) {
    ++lineno;
    auto text = Trim(std::string_view(line).substr(0, line.find(';')));
    if (text.starts_with(".program ")) {
      module = true;
      auto next = Trim(std::string_view(text).substr(9));
      if (active || plain || next.empty() || next.find_first_of(" \t") != next.npos || !names.emplace(next, true).second) {
        result.diagnostics.push_back({lineno, "nested, duplicate or invalid program"});
        continue;
      }
      name = next;
      body.clear();
      start = lineno;
      active = true;
    } else if (text == ".end") {
      if (!active) {
        result.diagnostics.push_back({lineno, "unexpected .end"});
        continue;
      }
      auto program = Assemble(body, symbols);
      for (auto diagnostic : program.diagnostics) {
        diagnostic.line += start;
        diagnostic.message = name + ": " + diagnostic.message;
        result.diagnostics.push_back(std::move(diagnostic));
      }
      result.programs.push_back({name, std::move(program.trace)});
      active = false;
    } else if (active)
      body += line + '\n';
    else if (!text.empty()) {
      if (module)
        result.diagnostics.push_back({lineno, "instruction outside a program"});
      else
        plain = true;
    }
  }
  if (active) result.diagnostics.push_back({start, "program is missing .end"});
  if (!module) {
    auto program = Assemble(source, symbols);
    result.diagnostics = std::move(program.diagnostics);
    result.programs.push_back({"", std::move(program.trace)});
  }
  if (!result.ok()) result.programs.clear();
  return result;
}
bool AssemblyResult::ok() const { return diagnostics.empty(); }
bool ModuleResult::ok() const { return diagnostics.empty(); }
}  // namespace openjoey::lacooda64::assembly
