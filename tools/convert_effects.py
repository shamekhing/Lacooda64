#!/usr/bin/env python3
"""Reproducibly compile every source effect into native instruction sequences."""
import argparse,json,hashlib
from pathlib import Path
from effect_source import parse
from effect_compile import Compiler,Schema
ROOT=Path(__file__).resolve().parents[1]
def convert():
    source=(ROOT/'data/effects.txt').read_text();effects=parse(source);schema=Schema()
    # Stable base value domains used by arithmetic mappings.
    for name in ('HEADS','TAILS'):schema.enum('COIN',name)
    for name in ('FACE_UP_ATTACK','FACE_UP_DEFENSE','FACE_DOWN_DEFENSE'):schema.enum('POSITION',name)
    compiled=[];manifest=[]
    for effect in effects:
        compiler=Compiler(effect,schema);body=compiler.finish()
        key=f'{effect.card}_{effect.number}'
        compiled.extend([f'; {effect.name}: effect {effect.number} ({effect.kind}, optional={int(effect.optional)})',f'.program {key}','PC Instruction Meaning',body,'.end',''])
        manifest.append({'program':key,'card_id':effect.card,'name':effect.name,'effect':effect.number,'kind':effect.kind,'optional':effect.optional,'source_line':effect.line,'source_statements':sorted(compiler.source_lines),'instruction_count':len(compiler.code),'inputs':{name:{'register':ref.operand,'type':ref.kind} for name,ref in compiler.inputs.items()},'warnings':compiler.warnings})
    header=['; Generated from data/effects.txt by tools/convert_effects.py.', '; Native instruction sequences only. See effects.bindings.json for the numeric state ABI.', '; Each .program has independent PCs, registers and labels. No effect dispatcher is used.','']
    binding={'format':1,'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'fields':schema.fields,'enums':schema.enums,'events':schema.events,'zones':schema.zones,'programs':manifest}
    return '\n'.join(header+compiled),json.dumps(binding,indent=2,ensure_ascii=False)+'\n'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
    output,bindings=convert()
    for path,text in [(ROOT/'data/effects.lasm',output),(ROOT/'data/effects.bindings.json',bindings)]:
        if args.check:
            if not path.exists() or path.read_text()!=text:raise SystemExit(f'{path.name} is stale; run tools/convert_effects.py')
        else:path.write_text(text)
    info=json.loads(bindings);print(f"{len({p['card_id'] for p in info['programs']})} cards, {len(info['programs'])} programs, {sum(p['instruction_count'] for p in info['programs'])} instructions")
if __name__=='__main__':main()
