"""Bounded Sign source accounting; runtime tests are explicit, never inferred."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CONFIG=ROOT/'config/decomp/sign_free_agent.json'
EXPECTED={'0x80056F9C':22,'0x80056D6C':53,'0x80056E40':87,'0x80057B00':27}

def calculate(c):
    owners=c['functions'];tests=c['core_tests']
    if len(owners)!=4 or {f['address']:f['instruction_count'] for f in owners}!=EXPECTED or c['instruction_total']!=189:
        raise ValueError('changed Sign denominator/owners')
    if len(set(tests))!=len(tests):raise ValueError('duplicate scenarios')
    source=(ROOT/c['source']).read_text();test_source=(ROOT/c['test_source']).read_text()
    for t in tests:
        if f'pass("{t}")' not in test_source:raise ValueError('scenario absent from native tests')
    accounted=0;rows=[]
    for f in owners:
        if f['symbol'] not in source:raise ValueError('missing native owner')
        start=int(f['address'],0);size=f['size_bytes'];seen=set();credit=0
        if size!=4*f['instruction_count']:raise ValueError('byte/count mismatch')
        for b in f['blocks']:
            a,z=int(b['start'],0),int(b['end'],0);words=set(range(a,z+1,4))
            if a%4 or z%4!=3 or a<start or z>=start+size or len(words)!=b['instruction_count'] or seen&words:
                raise ValueError('overlap/outside/invalid original block')
            seen|=words
            if b['accounted']:
                if not b['behavior'] or not b['tests'] or not set(b['tests'])<=set(tests):raise ValueError('credit without evidence')
                credit+=len(words)
        if seen!=set(range(start,start+size,4)):raise ValueError('incomplete original body')
        accounted+=credit;rows.append({'name':f['name'],'address':f['address'],'total':f['instruction_count'],'accounted':credit,'pending':f['instruction_count']-credit})
    return {'scope':c['scope'],'total':189,'accounted':accounted,'pending':189-accounted,
        'functions':rows,'static_report_executes_tests':False,'limitations':c['limitations']}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check',action='store_true');p.add_argument('--native-test',type=Path)
    p.add_argument('--fresh-inventory',type=Path)
    args=p.parse_args();c=json.loads(CONFIG.read_text());result=calculate(c)
    if args.fresh_inventory:
        original=json.loads(args.fresh_inventory.read_text())['functions']
        if len(original)!=4:raise ValueError('wrong fresh function count')
        by={f['address']:f for f in original}
        for f in c['functions']:
            old=by[f['address']]
            if any(old[k]!=f[k] for k in ('size_bytes','instruction_count')) or old['blocks']!=[
                {k:b[k] for k in ('start','end','instruction_count')} for b in f['blocks']]:raise ValueError('fresh original inventory drift')
    report=ROOT/'reports/sign_free_agent.json';rendered=json.dumps(result,indent=2)+'\n'
    if args.check:
        if report.read_text()!=rendered:raise ValueError('stale Sign report')
    else:report.write_text(rendered)
    if args.native_test:
        exe=args.native_test.resolve(strict=True)
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
        print(run.stdout,end='');actual=re.findall(r'^SIGN PASS (.+)$',run.stdout,re.M)
        if run.returncode or sorted(actual)!=sorted(c['core_tests']):raise ValueError('native Sign regressions failed/incomplete: '+run.stderr)
        paths=[CONFIG,ROOT/c['source'],ROOT/c['test_source'],ROOT/'src/recovered/roster_sign.h',
            ROOT/'src/recovered/roster_trade.c',ROOT/'src/recovered/roster_trade.h',ROOT/'src/recovered/roster_lists.c',ROOT/'src/recovered/roster_reorder.c']
        evidence={'passed':actual,'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),
            'source_sha256':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}}
        target=ROOT/'.local/verification/sign-core.json';target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(json.dumps(evidence,indent=2)+'\n')
    print(f"SIGN ACCOUNTING {result['accounted']}/189; pending={result['pending']}; bounded source contracts, not feature fidelity")
if __name__=='__main__':main()
