"""Bounded Reset source contracts; never a whole-feature fidelity percentage."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT/'config/decomp/reset_rosters.json'
COUNTS = {'0x80057864': 63, '0x80057960': 66, '0x80057C48': 20,
          '0x80058104': 32, '0x800582C4': 56}
REVIEWED = {'0x80057C48': 'nba97_reset_enabled', '0x80058104': 'nba97_reset_table_differs'}


def calculate(config):
    owners = config['functions']
    if (len(owners) != len(COUNTS) or config['instruction_total'] != 237 or
            {f['address']: f['instruction_count'] for f in owners} != COUNTS):
        raise ValueError('Reset scope/denominator drift')
    source = (ROOT/config['source']).read_text()
    test_source = (ROOT/config['test_source']).read_text()
    tests = config['core_tests']
    if not tests or len(tests) != len(set(tests)) or any('RESET PASS '+t not in test_source for t in tests):
        raise ValueError('missing or duplicate Reset tests')
    rows = []
    for owner in owners:
        start = int(owner['address'], 0)
        size = owner['size_bytes']
        if size != owner['instruction_count']*4:
            raise ValueError('Reset body size drift')
        seen, credit = set(), 0
        for block in owner['blocks']:
            first, last = int(block['start'], 0), int(block['end'], 0)
            words = set(range(first, last+1, 4))
            if (first % 4 or last % 4 != 3 or first < start or last >= start+size or
                    not words or len(words) != block['instruction_count'] or words & seen):
                raise ValueError('invalid Reset block extent/count')
            seen |= words
            if type(block['accounted']) is not bool:
                raise ValueError('Reset credit must be boolean')
            if block['accounted']:
                symbol = owner['symbol']
                if (owner['address'] not in REVIEWED or symbol != REVIEWED[owner['address']] or
                        not symbol or symbol not in source or not block['behavior'] or
                        not block['tests'] or not set(block['tests']) <= set(tests)):
                    raise ValueError('unreviewed Reset credit')
                credit += len(words)
        if seen != set(range(start, start+size, 4)):
            raise ValueError('incomplete Reset block inventory')
        rows.append(dict(address=owner['address'], name=owner['name'],
                         total=owner['instruction_count'], accounted=credit,
                         pending=owner['instruction_count']-credit))
    credit = sum(row['accounted'] for row in rows)
    return dict(scope=config['scope'], total=237, accounted=credit, pending=237-credit,
                functions=rows, static_report_executes_tests=False, limitations=config['limitations'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--native-test', type=Path)
    parser.add_argument('--fresh-inventory', type=Path)
    args = parser.parse_args()
    config = json.loads(CONFIG.read_text())
    report = calculate(config)
    if args.fresh_inventory:
        fresh = {f['address']: f for f in json.loads(args.fresh_inventory.read_text())['functions']}
        for f in config['functions']:
            original = fresh[f['address']]
            if (any(f[k] != original[k] for k in ('instruction_count', 'size_bytes')) or
                    original['blocks'] != [{k: b[k] for k in ('start', 'end', 'instruction_count')}
                                           for b in f['blocks']]):
                raise ValueError('fresh Reset inventory mismatch')
    path = ROOT/'reports/reset_rosters.json'
    rendered = json.dumps(report, indent=2)+'\n'
    if args.check:
        if path.read_text() != rendered:
            raise ValueError('stale Reset report')
    else:
        path.write_text(rendered)
    if args.native_test:
        run = subprocess.run([str(args.native_test.resolve(strict=True))],
                             capture_output=True, text=True, timeout=30)
        print(run.stdout, end='')
        passed = re.findall(r'^RESET PASS (.+)$', run.stdout, re.M)
        if run.returncode or any(passed.count(t) != 1 for t in config['core_tests']):
            raise ValueError('Reset native scenarios failed/missing: '+run.stderr)
    print(f"RESET ACCOUNTING {report['accounted']}/237; pending={report['pending']}; "
          'source contracts only; normal host function and fidelity tracked separately')


if __name__ == '__main__':
    main()
