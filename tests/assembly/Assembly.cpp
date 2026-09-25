#include <iostream>

#include "assembly/Assembler.hpp"
#include "assembly/Disassembler.hpp"
using namespace openjoey::lacooda64;
using namespace openjoey::lacooda64::assembly;
int main() {
  bool ok = true;
  auto check = [&](bool value, const char* message) {
    if (!value) {
      std::cerr << message << '\n';
      ok = false;
    }
  };
  auto program = Assemble(R"(
.address P0.MONSTERS [0:1:0]
.address P0.DECK [0:2:0]
.address P0.HAND [0:3:0]
PC Instruction Meaning
0 COUNT V0, P0.MONSTERS ; count
1 COMPARE F0, V0, #3, GREATER_EQUAL
2 JUMPIF F0, draw
3 HALT
draw:
4 MOVE DRAW, P0.HAND, P0.DECK, #2
5 HALT
)");
  check(program.ok() && program.trace.size() == 6, "sample notation");
  check(Disassemble(program.trace).find("MOVE DRAW") != std::string::npos, "readable instruction formatting");
  auto restored = Assemble(Disassemble(program.trace));
  check(restored.ok() && restored.trace == program.trace, "lossless text roundtrip");
  auto indirect = Assemble(
      "SELECT A0, [0:1:0]\nENUMERATE V0, A0\nAT A1, V0, #0\nATTRIBUTE A2, A1, "
      "#1\nLOAD V1, A2\nHALT");
  check(indirect.ok(), "numeric query primitives");
  for (auto source : {".constant @0 subject=self\nHALT", "0 JUMP absent", "0 JUMP 99", "1 HALT", "ALU V0, #1, #2, BOGUS", "COUNT V999, [0]", "COUNT V0, [99999:0]", "SET V0, #99999999999999999999999", "ENUMERATE A0, [0]", ".symbol X #1\n.symbol X #2\nHALT", "HALT\nBOGUS"}) {
    auto invalid = Assemble(source);
    check(!invalid.ok() && invalid.trace.empty(), "malformed input must not produce executable prefix");
  }
  auto pending = Assemble("STAGE V0, 3, #30\nCOMMIT V0\nHALT\nMOVE DESTROY, [0:1:0], [0:2:0]\nHALT");
  check(pending.ok() && Assemble(Disassemble(pending.trace)).trace == pending.trace, "pending instruction roundtrip");
  check(Assemble("STORE CONTROL.PENDING_CANCELLED, #1\nHALT").ok(), "pending context symbol");
  check(!Assemble("STAGE V0, 9999, #30\nHALT").ok(), "staged instruction target bounds");
  auto metadata = Assemble("MOVE SEND, [0:1:0], [0:2:0], #1 | flags=4 cause=3 aux=9\nHALT");
  check(metadata.ok() && FlagsOf(Operation(metadata.trace[0])) == 4 && AuxOf(Operation(metadata.trace[0])) == 9, "checked metadata encoding");
  check(metadata.ok() && Assemble(Disassemble(metadata.trace)).trace == metadata.trace, "readable metadata roundtrip");
  for (auto text : {"HALT | flags=4096", "HALT | cause=256", "HALT | aux=65536", "HALT | flags=1 flags=2", "HALT | unknown=0", "HALT |"}) check(!Assemble(text).ok(), "invalid metadata rejected");
  auto module = AssembleModule(".program a\n0 HALT\n.end\n.program b\n0 HALT\n.end");
  check(module.ok() && module.programs.size() == 2, "independent program PCs");
  for (auto text : {".program a\nHALT", ".end", ".program a\n.program b\n.end", ".program a\nHALT\n.end\n.program a\nHALT\n.end", ".program a\nHALT\n.end\nHALT", "HALT\n.program a\nHALT\n.end"}) {
    auto invalid = AssembleModule(text);
    check(!invalid.ok() && invalid.programs.empty(), "malformed module never exposes partial programs");
  }
  check(!AssembleModule(".program a\nJUMP elsewhere\n.end\n.program b\nelsewhere:\nHALT\n.end").ok(), "labels cannot cross program boundaries");
  return ok ? 0 : 1;
}
