# Tools

`convert_effects.py` compiles all structured blocks in `data/effects.txt` into
`data/effects.lasm` and `data/effects.bindings.json`. Run it from any directory:

```sh
python3 tools/convert_effects.py
python3 tools/convert_effects.py --check
```

The converter requires Python 3.10+ and no third-party packages. `effect_source.py`
parses the dataset; `effect_compile.py` lowers it to native instructions.
Unknown operations and unsupported constructs fail with source locations.
Generation finishes in memory before updating output files. `--check` changes
nothing and fails when either output is stale.

`lacooda-asm.cpp` builds as `lacooda-asm` with `LACOODA64_BUILD_TOOLS=ON`:

```sh
build/lacooda-asm data/effects.lasm
build/lacooda-asm data/effects.lasm /tmp/canonical-effects.lasm
```

It accepts single programs and `.program` / `.end` modules, validates all operand
and branch encodings, and optionally writes a lossless text listing. PCs restart
at zero for each program. A failed module never exposes a partial executable
result through the assembly API.

Dataset-specific source handling stays here. The ISA, runtime and assembler do
not know card names or interpret source action records. See the
[corpus guide](../data/README.md) for the binding contract and source diagnostics.
