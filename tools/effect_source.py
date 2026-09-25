"""Parser for the supplied effect-source notation; independent of the ISA."""
from dataclasses import dataclass
import re

@dataclass
class Call:
    name: str
    args: object

@dataclass
class Node:
    op: str
    args: object
    line: int
    children: object = None
    otherwise: object = None

@dataclass
class Effect:
    card: int
    name: str
    number: int
    kind: str
    optional: bool
    line: int
    nodes: list


def split(text, separator=','):
    out, start, depth, quote = [], 0, 0, False
    for i, c in enumerate(text):
        if c == '"': quote = not quote
        if quote: continue
        if c in '([{': depth += 1
        elif c in ')]}': depth -= 1
        if c == separator and depth == 0:
            out.append(text[start:i].strip()); start = i + 1
        if depth < 0: raise ValueError('unbalanced value: ' + text)
    if depth or quote: raise ValueError('unclosed value: ' + text)
    if text[start:].strip(): out.append(text[start:].strip())
    return out


def record(text):
    text = text.strip()
    if text.startswith('{') and text.endswith('}'): text = text[1:-1]
    marks, depth, quote = [], 0, False
    for i, c in enumerate(text):
        if c == '"': quote = not quote
        if quote: continue
        if depth == 0 and (i == 0 or text[i-1] in ' ,\t'):
            m = re.match(r'([A-Za-z_][\w.]*)=', text[i:])
            if m: marks.append((i, i+len(m[0]), m[1]))
        if c in '([{': depth += 1
        elif c in ')]}': depth -= 1
    if depth or quote: raise ValueError('unbalanced record: ' + text)
    if not marks: raise ValueError('expected key=value record: ' + text)
    out = {}
    for n, (start, end, key) in enumerate(marks):
        if key in out: raise ValueError('duplicate field: ' + key)
        stop = marks[n+1][0] if n+1 < len(marks) else len(text)
        atom = text[end:stop].strip().rstrip(',').strip()
        if not atom: raise ValueError('empty field: ' + key)
        out[key] = value(atom)
    return out


def value(text):
    text = text.strip()
    if text.startswith('{') and text.endswith('}'): return record(text)
    if text.startswith('[') and text.endswith(']'): return [value(x) for x in split(text[1:-1])]
    m = re.fullmatch(r'([A-Z_]+)(\(.*\)|\{.*\})', text, re.S)
    if m:
        body = m[2]
        return Call(m[1], record(body) if body.startswith('{') else [value(x) for x in split(body[1:-1])])
    return text


def statement(text, line):
    # Two missing spaces in the input are lexical errors, not new operations.
    text = re.sub(r'^(DECLARE_RITUAL_COMPATIBILITY|SHUFFLE_FIELD_IDENTITIES)(?=subject)', r'\1 ', text)
    parts = text.split(None, 1); op = parts[0]; tail = parts[1] if len(parts)>1 else ''
    if op in ('TEST', 'CMP/JIF'): args = value(tail)
    elif op in ('EVENT.MATCH', 'REQUIRE_EVENT'):
        args = value(tail) if op == 'EVENT.MATCH' else record(tail)
    elif op == 'JIF_FALSE': args = tail
    elif tail and not tail.startswith(';'): args = record(tail)
    else: args = {}
    return Node(op, args, line)


def parse_body(lines):
    def sequence(i, indent=0, brace=False):
        nodes = []
        while i < len(lines):
            n, spacing, text = lines[i]
            if text == '}':
                if not brace: break
                return nodes, i+1
            if spacing < indent: break
            if text.startswith('ELSE') or text.startswith('CASE '): break
            if text == '{': raise ValueError(f'line {n}: unexpected opening brace')
            if text.startswith('CMP/JIF '):
                node = statement(text,n); i += 1
                if i == len(lines) or lines[i][2] != '{': raise ValueError(f'line {n}: missing conditional body')
                node.children, i = sequence(i+1,0,True)
                if i < len(lines) and lines[i][2].startswith('ELSE'):
                    node.otherwise, i = sequence(i+1,0,True)
                nodes.append(node); continue
            if text.startswith('CHOICE '):
                node=statement(text,n); cases=[]; i+=1
                while i<len(lines) and lines[i][2].startswith('CASE '):
                    cn,cs,ct=lines[i]; label=int(ct[5:].rstrip(':'))
                    body,i=sequence(i+1,cs+1)
                    cases.append((label,body))
                if not cases: raise ValueError(f'line {n}: choice has no cases')
                node.children=cases;nodes.append(node);continue
            if text=='COST:' or text.startswith('SCHEDULE '):
                node=statement(text,n); node.children,i=sequence(i+1,spacing+1);nodes.append(node);continue
            nodes.append(statement(text,n));i+=1
        if brace: raise ValueError('unclosed conditional block')
        return nodes,i
    nodes,end=sequence(0)
    if end != len(lines): raise ValueError(f'line {lines[end][0]}: unconsumed source')
    return nodes


def parse(source):
    lines=source.splitlines(); effects=[];card=name=None;i=0
    while i<len(lines):
        line=lines[i]
        m=re.match(r'CARD\s+\d+\s+(.+?)\s+\[',line)
        if m: name=m[1]
        if line.startswith('ID   '): card=int(line.split()[1])
        m=re.match(r'  EFFECT\s+(\d+)\s+(\w+)',line)
        if m:
            start=i;i+=1;body=[]
            while i<len(lines) and not lines[i].startswith(('CARD ','  EFFECT ')):
                text=lines[i].strip()
                if text: body.append((i+1,len(lines[i])-len(lines[i].lstrip()),text))
                i+=1
            nodes=parse_body(body)
            optional=any(n.op=='FLAGS' and n.args.get('optional')=='1' for n in nodes)
            effects.append(Effect(card,name,int(m[1]),m[2],optional,start+1,nodes));continue
        i+=1
    if not effects: raise ValueError('no source effect blocks found')
    keys=[(e.card,e.number) for e in effects]
    if len(keys)!=len(set(keys)): raise ValueError('duplicate card/effect entry')
    return effects
