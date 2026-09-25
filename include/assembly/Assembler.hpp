#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "Diagnostics.hpp"
#include "Symbols.hpp"
#include "lacooda/Machine.hpp"
namespace openjoey::lacooda64::assembly {
struct AssemblyResult {
  Trace trace;
  std::vector<Diagnostic> diagnostics;
  bool ok() const;
};
// Two-pass, checked text assembler. Failure never returns an executable prefix.
AssemblyResult Assemble(std::string_view source, const Symbols& symbols = {});
struct Program {
  std::string name;
  Trace trace;
};
struct ModuleResult {
  std::vector<Program> programs;
  std::vector<Diagnostic> diagnostics;
  bool ok() const;
};
// Each .program NAME / .end block has an independent PC and symbol scope.
// Plain single-program text is accepted as the unnamed program.
ModuleResult AssembleModule(std::string_view source, const Symbols& symbols = {});
}  // namespace openjoey::lacooda64::assembly
