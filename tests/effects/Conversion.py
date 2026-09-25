"""Regression checks for the source-to-native conversion, using the real corpus."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from convert_effects import convert
from effect_source import parse, parse_body, statement, Effect, Node, Call
from effect_compile import Compiler, Schema


class ConversionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.effects = parse((ROOT / 'data/effects.txt').read_text())
        cls.output, manifest = convert()
        cls.manifest = json.loads(manifest)

    def test_every_source_block_and_statement_is_visited(self):
        def lines(nodes):
            result = set()
            for node in nodes:
                result.add(node.line)
                if node.children:
                    if node.op == 'CHOICE':
                        for _, body in node.children:
                            result.update(lines(body))
                    else:
                        result.update(lines(node.children))
                result.update(lines(node.otherwise or []))
            return result
        self.assertEqual(len(self.effects), 264)
        self.assertEqual(len({e.card for e in self.effects}), 120)
        for effect, program in zip(self.effects, self.manifest['programs']):
            self.assertEqual(program['program'], f'{effect.card}_{effect.number}')
            self.assertEqual(set(program['source_statements']), lines(effect.nodes))

    def test_output_is_reproducible_and_contains_no_opaque_actions(self):
        self.assertEqual((self.output, json.dumps(self.manifest, indent=2, ensure_ascii=False)+'\n'), convert())
        for legacy in ('.constant', '.effect', 'FX_', 'QUERY{', 'REF(', 'duration='):
            self.assertNotIn(legacy, self.output)
        self.assertEqual(self.output.count('.program '), 265)  # includes format comment
        self.assertEqual(self.output.count('\n.end'), 264)

    def test_source_gaps_remain_visible(self):
        warnings = [w for p in self.manifest['programs'] for w in p['warnings']]
        self.assertEqual(sum('no resolution' in w['message'] for w in warnings), 2)
        self.assertEqual(sum('no candidate set' in w['message'] for w in warnings), 2)
        self.assertEqual(sum('declared address input' in w['message'] for w in warnings), 4)
        for program in self.manifest['programs']:
            self.assertNotIn('DUEL', program['inputs'])

    def test_unknown_operations_and_predicates_fail_loudly(self):
        for node in (Node('UNKNOWN', {}, 1), Node('TEST', {'pred':'UNKNOWN'}, 1)):
            with self.assertRaisesRegex(ValueError, 'unhandled|unknown'):
                Compiler(Effect(1, 'fixture', 1, 'ACTIVATION', False, 1, [node]), Schema()).finish()

    def test_unconsumed_and_duplicate_source_is_rejected(self):
        with self.assertRaises(ValueError):
            parse_body([(1, 4, 'HALT.EFFECT'), (2, 4, '}')])
        with self.assertRaises(ValueError):
            statement('MOVE subject=self subject=other', 1)
        source = 'CARD 001 Test [SPELL]\nID   1\n  EFFECT 1 ACTIVATION\n    HALT.EFFECT\n'
        with self.assertRaises(ValueError):
            parse(source + source)

    def test_metadata_retains_maintenance_and_visibility(self):
        self.assertIn('flags=68 cause=6', self.output)
        self.assertIn('flags=32 cause=0', self.output)


if __name__ == '__main__':
    unittest.main()
