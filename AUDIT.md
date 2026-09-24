# Encoding and library audit

Reviewed the word layouts, address/register/immediate/control helpers, operation
packing, instruction builders, operand/trace validation, serialization, CMake,
tests, and README against the current configuration.

## Fixed

| Finding | Resolution |
| --- | --- |
| Address fixtures assumed 3-bit levels and 2-bit players after both changed to 4 bits | Replaced fixed words with an independent bit-by-bit reference using field widths; test maxima and overflow from masks |
| Reserved-bit assertions pinned a particular allocation | Derive remaining address/operation space and reject layouts exceeding the payload |
| Width changes could silently truncate supported enum values | Added compile-time field-size and enum-capacity checks |
| Register storage remained 256 entries if the index width changed | Derive `kRegisterCount` and all three arrays from the index width |
| Oversized Jump/JumpIf targets could truncate to a valid instruction index | Encode unrepresentable targets as immediate -1 so trace validation rejects them |
| Allocating `Decode` was `noexcept`, making allocation failure terminate | Allow allocation exceptions to propagate, consistent with the encoders |
| Address documentation still specified 53 used bits and players 0..3 | Updated to 56 used bits, 4 reserved, and players 0..15 |
| Example assembly listed Move's source and destination in the opposite order to the README | Standardized the example to destination, source |

## Validation scope retained

Validation is structural and intentionally remains permissive in several places:

- Address/register/operation reserved bits are not required to be zero.
- Shallow addresses may contain nonzero deeper fields.
- Most subcode enums are not range-checked; Event accepts any nonzero subcode.
- Some instructions require only present operands rather than enforcing value
  types or empty unused slots.
- Game-state existence, arithmetic behavior, and termination belong to the host.

These are existing documented contracts, not newly tightened acceptance rules.
A consumer requiring canonical encodings or strict enum validation must add those
checks before interpreting external input. There is still no format identifier or
legacy-layout detection in the word stream.

## Verification

- Compile-time checks for field isolation, sequential/direct decoding agreement,
  signed immediate boundaries, truncation, register storage, and enum handling.
- Runtime checks for word/byte serialization, trace round trips, branch bounds,
  empty programs, alignment errors, and partial decode results.
- Builds with warnings treated as errors.
- Tests against temporary copies using the former address widths, an address
  layout using all payload bits, wider register indices/banks, rebalanced
  operation fields, and a wider tag. All five configurations passed.
- An overfull address layout was rejected by the expected compile-time diagnostic.

The audit does not establish execution correctness: this repository defines an
encoding library and has no interpreter. Field widths remain a build-time format
choice; updating them requires coordinated producers/consumers and documentation.
