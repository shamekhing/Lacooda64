#include "runtime/StateAccess.hpp"
namespace openjoey::lacooda64::runtime {
bool StateAccess::Create(Word, Address, Address&) { return false; }
bool StateAccess::Exchange(Address, Address, Address&, Address&) { return false; }
}  // namespace openjoey::lacooda64::runtime
