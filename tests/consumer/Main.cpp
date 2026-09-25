#include "assembly/Assembler.hpp"
#include "runtime/Runtime.hpp"
using namespace openjoey::lacooda64;
Trace ConsumerTrace();
class Storage final : public runtime::StateAccess {
 public:
  bool Read(Address, Word&) const override { return false; }
  bool Write(Address, Word) override { return false; }
  bool Members(Address, std::vector<Address>&) const override { return false; }
  bool Relocate(Address, Address, Address&) override { return false; }
};
int main() {
  const auto trace = ConsumerTrace();
  if (trace.empty()) return 1;
  Storage storage;
  std::mt19937 rng(42);
  runtime::Runtime engine(storage, rng);
  runtime::Frame frame(trace);
  if (engine.Run(frame).status != runtime::Status::kHalted || frame.machine.regs.value[0] != Imm(42)) return 2;
  return assembly::Assemble("SET V0, #42\nHALT").trace == trace ? 0 : 3;
}
