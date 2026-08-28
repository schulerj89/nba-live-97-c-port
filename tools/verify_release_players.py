"""Release-owned source accounting; execution and visual fidelity are separate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / 'config/decomp/release_players.json'
EXPECTED = {'0x80056FF4': 36, '0x80057084': 102, '0x8005721C': 53, '0x80057B6C': 17}
REVIEWED_SYMBOLS = {'0x80056FF4': 'nba97_release_advance', '0x80057084': 'nba97_release_callback',
                    '0x8005721C': 'nba97_release_begin', '0x80057B6C': 'nba97_release_available'}


def calculate(config):
    owners = config['functions']
    tests = config['core_tests']
    if len(owners) != 4 or {f['address']: f['instruction_count'] for f in owners} != EXPECTED:
        raise ValueError('Release denominator/owners changed')
    if config['instruction_total'] != 208 or len(set(tests)) != len(tests) or not tests:
        raise ValueError('bad total or duplicate/empty scenario list')
    source = (ROOT / config['source']).read_text()
    test_source = (ROOT / config['test_source']).read_text()
    if any(f'pass("{name}")' not in test_source for name in tests):
        raise ValueError('missing native scenario')
    rows = []
    for function in owners:
        start = int(function['address'], 0)
        size = function['size_bytes']
        if size != function['instruction_count'] * 4:
            raise ValueError('body size mismatch')
        seen = set()
        credited = 0
        for block in function['blocks']:
            first, last = int(block['start'], 0), int(block['end'], 0)
            words = set(range(first, last + 1, 4))
            if (first % 4 or last % 4 != 3 or first < start or last >= start + size
                    or not words or len(words) != block['instruction_count'] or seen & words):
                raise ValueError('invalid/overlapping/outside block')
            seen |= words
            if type(block['accounted']) is not bool:
                raise ValueError('accounted must be boolean')
            if block['accounted']:
                if (not function['symbol'] or function['symbol'] not in source or
                        not block['behavior'] or not block['tests'] or not set(block['tests']) <= set(tests)):
                    raise ValueError('credit without native evidence')
                # Reviewed against FEONLY recomp and true-entry Ghidra inventory.
                # Reject unrelated existing symbols, not just absent names.
                if function['symbol'] != REVIEWED_SYMBOLS[function['address']]:
                    raise ValueError('block has not passed Release source review')
                credited += len(words)
        if seen != set(range(start, start + size, 4)):
            raise ValueError('incomplete body inventory')
        rows.append({'name': function['name'], 'address': function['address'],
                     'total': function['instruction_count'], 'accounted': credited,
                     'pending': function['instruction_count'] - credited})
    total_credit = sum(row['accounted'] for row in rows)
    return {'scope': config['scope'], 'total': 208, 'accounted': total_credit,
            'pending': 208-total_credit, 'functions': rows,
            'static_report_executes_tests': False, 'limitations': config['limitations']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--native-test', type=Path)
    parser.add_argument('--fresh-inventory', type=Path)
    args = parser.parse_args()
    config = json.loads(CONFIG.read_text())
    result = calculate(config)
    if args.fresh_inventory:
        fresh = json.loads(args.fresh_inventory.read_text())['functions']
        if len(fresh) != 4:
            raise ValueError('wrong fresh function count')
        by_address = {function['address']: function for function in fresh}
        for function in config['functions']:
            original = by_address[function['address']]
            if (any(original[k] != function[k] for k in ('size_bytes', 'instruction_count')) or
                    original['blocks'] != [{k: block[k] for k in ('start', 'end', 'instruction_count')}
                                           for block in function['blocks']]):
                raise ValueError('fresh original inventory drift')
    path = ROOT / 'reports/release_players.json'
    rendered = json.dumps(result, indent=2) + '\n'
    if args.check:
        if path.read_text() != rendered:
            raise ValueError('stale Release report')
    else:
        path.write_text(rendered)
    if args.native_test:
        exe = args.native_test.resolve(strict=True)
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        print(run.stdout, end='')
        actual = re.findall(r'^RELEASE PASS (.+)$', run.stdout, re.M)
        if run.returncode or sorted(actual) != sorted(config['core_tests']):
            raise ValueError('native scenarios failed/incomplete: ' + run.stderr)
        paths = [CONFIG, ROOT/config['source'], ROOT/config['test_source'],
                   ROOT/'src/recovered/roster_release.h', ROOT/'src/recovered/roster_trade.c',
                   ROOT/'src/recovered/roster_trade.h', ROOT/'src/recovered/roster_reorder.c',
                   ROOT/'src/recovered/roster_lists.c']
        evidence = {'passed': actual, 'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
                    'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                      for p in paths}}
        target = ROOT / '.local/verification/release-core.json'
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(evidence, indent=2) + '\n')
    print(f"RELEASE ACCOUNTING {result['accounted']}/208; pending={result['pending']}; "
          'source contracts only; original runtime/animation/audio acceptance separate')


if __name__ == '__main__':
    main()
