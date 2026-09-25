#pragma once
#include <map>
#include <string>

#include "lacooda/Word.hpp"
namespace openjoey::lacooda64::assembly {
// Explicit project/ruleset bindings; no hardcoded player, zone or field IDs.
using Symbols = std::map<std::string, Operand, std::less<>>;
}  // namespace openjoey::lacooda64::assembly
