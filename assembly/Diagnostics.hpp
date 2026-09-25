#pragma once
#include <cstddef>
#include <string>
namespace openjoey::lacooda64::assembly {
struct Diagnostic {
  std::size_t line;
  std::string message;
};
}  // namespace openjoey::lacooda64::assembly
