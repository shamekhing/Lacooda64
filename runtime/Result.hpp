#pragma once
#include <string>

#include "lacooda/Word.hpp"
namespace openjoey::lacooda64::runtime {
enum class Status { kHalted, kWaiting, kStepLimit, kInvalidProgram, kFault, kUnsupported };
struct Result {
  Status status;
  Word pc;
  Word steps;
  std::string message;
};
}  // namespace openjoey::lacooda64::runtime
