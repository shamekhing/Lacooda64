#include "lacooda/Machine.hpp"

namespace openjoey::lacooda64 {
ProgramWords Encode(const Trace& trace) {
  ProgramWords out;
  out.reserve(trace.size() * 4);
  for (const auto& i : trace) {
    out.insert(out.end(), i.begin(), i.end());
  }
  return out;
}
DecodeResult Decode(const ProgramWords& words) {
  DecodeResult out{};
  if ((words.size() % 4) != 0) {
    out.error_ = DecodeError::kMisalignedWordCount;
    out.word_offset_ = static_cast<Word>(words.size());
    return out;
  }

  out.trace_.reserve(words.size() / 4);
  for (std::size_t i = 0; i < words.size(); i += 4) {
    Instruction inst{words[i], words[i + 1], words[i + 2], words[i + 3]};
    if (!ValidInstruction(inst)) {
      out.error_ = DecodeError::kInvalidInstruction;
      out.word_offset_ = static_cast<Word>(i);
      return out;
    }
    out.trace_.push_back(inst);
  }
  return out;
}
void WriteU64Le(Bytecode& out, Word w) {
  for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((w >> (i * 8)) & 0xFF));
}
Bytecode EncodeBytes(const Trace& trace) {
  Bytecode out;
  out.reserve(trace.size() * 32);
  for (const auto& inst : trace)
    for (Word w : inst) WriteU64Le(out, w);
  return out;
}
}  // namespace openjoey::lacooda64
