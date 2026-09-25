#include <fstream>
#include <iostream>
#include <iterator>

#include "assembly/Assembler.hpp"
#include "assembly/Disassembler.hpp"
int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: lacooda-asm INPUT [CANONICAL_OUTPUT]\n";
    return 2;
  }
  std::ifstream file(argv[1]);
  if (!file) {
    std::cerr << "cannot open input\n";
    return 2;
  }
  std::string source{std::istreambuf_iterator<char>(file), {}};
  auto result = openjoey::lacooda64::assembly::AssembleModule(source);
  for (const auto& error : result.diagnostics)
    std::cerr << argv[1] << ':' << error.line << ": " << error.message << '\n';
  if (!result.ok()) return 1;
  if (argc == 3) {
    std::ofstream out(argv[2]);
    for (const auto& program : result.programs) {
      if (!program.name.empty()) out << ".program " << program.name << '\n';
      out << openjoey::lacooda64::assembly::Disassemble(program.trace);
      if (!program.name.empty()) out << ".end\n";
    }
    if (!out) return 2;
  }
  std::size_t count = 0;
  for (const auto& program : result.programs) count += program.trace.size();
  std::cout << result.programs.size() << " programs, " << count << " instructions\n";
}
