#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>

#include "assembly/Assembler.hpp"
#include "assembly/Disassembler.hpp"
#include "runtime/Runtime.hpp"
#include "tests/support/MemoryState.hpp"
using namespace openjoey::lacooda64;
using namespace openjoey::lacooda64::assembly;
using namespace openjoey::lacooda64::runtime;
std::string Read(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot read " + path);
  return {std::istreambuf_iterator<char>(input), {}};
}
// The fixture needs only flat numeric maps from the generated manifest.
Word Binding(const std::string& json, const std::string& section, const std::string& key) {
  auto start = json.find('{' , json.find('"' + section + '"'));
  auto block = json.substr(start, json.find('}', start) - start);
  std::smatch match;
  if (!std::regex_search(block, match, std::regex('"' + key + "\": ([0-9]+)")))
    throw std::runtime_error("missing fixture binding " + section + "." + key);
  return std::stoull(match[1]);
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  bool ok = true;
  auto check = [&](bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; ok = false; }
  };
  const std::string root = argv[1];
  auto source = Read(root + "/data/effects.lasm");
  auto manifest = Read(root + "/data/effects.bindings.json");
  auto module = AssembleModule(source);
  for (const auto& error : module.diagnostics) std::cerr << error.line << ": " << error.message << '\n';
  check(module.ok() && module.programs.size() == 264, "all 264 source blocks assemble");
  std::set<std::string> cards;
  for (const auto& program : module.programs) {
    cards.insert(program.name.substr(0, program.name.find('_')));
    auto restored = Assemble(Disassemble(program.trace));
    check(restored.ok() && restored.trace == program.trace, "whole corpus text round trip");
    auto decoded = Decode(Encode(program.trace));
    check(decoded.ok() && decoded.trace_ == program.trace, "whole corpus word round trip");
  }
  check(cards.size() == 120, "all 120 cards retained");
  auto field = [&](const char* key) { return Binding(manifest, "fields", key); };
  auto zone = [&](const char* key) { return Binding(manifest, "zones", key); };
  const auto event = Binding(manifest, "events", "PHASE_BEGIN");
  // Run the actual generated Mirage program, including its phase predicate,
  // captured draw count, seeded sampling and one-shot cancellation.
  for (const auto& program : module.programs) if (program.name == "41482598_1") {
    test::MemoryState state;
    const auto hand = Zone(0, zone("HAND")), deck = Zone(0, zone("DECK")), grave = Zone(0, zone("GY"));
    state.zones[hand] = {Card(0, zone("HAND"), 0, 10)};
    state.zones[deck] = {Card(0, zone("DECK"), 0, 11), Card(0, zone("DECK"), 0, 12), Card(0, zone("DECK"), 0, 13), Card(0, zone("DECK"), 0, 14)};
    state.zones[grave] = {};
    std::mt19937 rng(17);
    Runtime runtime(state, rng);
    Frame frame(program.trace);
    frame.machine.regs.value[0] = Imm(0);
    frame.machine.regs.value[1] = Imm(1);
    frame.machine.control[ControlIndex(ControlReg::kEventKind)] = Imm(event);
    check(runtime.Run(frame).status == Status::kHalted, "generated draw body executes");
    check(state.zones[hand].size() == 4 && state.zones[deck].size() == 1, "generated draw reaches four cards");
    for (auto object : state.zones[hand]) state.fields[WithAttribute(object, field("OWNER"))] = Imm(0);
    // Determine the numeric phase enum without assuming its allocation order.
    auto enums = manifest.substr(manifest.find("\"enums\""));
    state.fields[Duel(field("PHASE"))] = Imm(Binding(enums, "PHASE", "STANDBY"));
    state.fields[Duel(field("TURN_PLAYER"))] = Imm(1);
    runtime.Deliver({event, Player(1), kNone, kNone});
    auto other = runtime.TakeReady();
    check(other.size() == 1, "subscription receives opponent phase for filtering");
    for (auto& pending : other) check(runtime.Run(pending).status == Status::kHalted, "nonmatching phase safely skips body");
    check(state.zones[grave].empty(), "opponent phase does not discard");
    state.fields[Duel(field("TURN_PLAYER"))] = Imm(0);
    runtime.Deliver({event, Player(0), kNone, kNone});
    auto ready = runtime.TakeReady();
    check(ready.size() == 1, "matching phase queues captured program");
    for (auto& pending : ready) {
      auto result = runtime.Run(pending);
      if (result.status != Status::kHalted) std::cerr << "generated Mirage stopped at PC " << pending.pc << '\n';
      check(result.status == Status::kHalted, "generated random discard executes");
    }
    check(state.zones[hand].size() == 1 && state.zones[grave].size() == 3, "generated body discards captured draw count without replacement");
    runtime.Deliver({event, Player(0), kNone, kNone});
    check(runtime.TakeReady().empty(), "generated one-shot subscription cancels itself");
  }
  return ok ? 0 : 1;
}
