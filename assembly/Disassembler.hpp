#pragma once
#include <string>

#include "lacooda/Machine.hpp"
namespace openjoey::lacooda64::assembly {
// Lossless canonical text, including operation metadata and reserved bits.
std::string Disassemble(const Trace& trace);
}  // namespace openjoey::lacooda64::assembly
