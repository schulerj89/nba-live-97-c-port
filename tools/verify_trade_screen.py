"""Fresh private Trade host regression, not original-execution equivalence."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
import re
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
CHECKPOINTS='entry help-first empty-first empty-view-notice empty-compare-notice second help-second cancel-second second-team view view-help view-team-scan view-layer view-browsed view-keep view-ignore compare-initial compare-initial-keep compare-initial-return view-return compare compare-help compare-layer compare-browsed compare-keep compare-return traded discard-after-trade view-after-trade compare-after-trade save-failed restart discard save-sync-uncertain'.split()
CHECKPOINTS+='quirk-retained-reopen-0 compare-retained transfer-receiver-empty transfer-second transfer-complete transfer-discard transfer-discard-reopened quirk-later-discard-1 quirk-cancel-save-failed quirk-retained-reopen-1 quirk-cancel-sync-uncertain quirk-retained-reopen-2 quirk-later-discard-3 quirk-retained-reopen-3'.split()

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def validate_summary(output):
    for marker in ('TRADE-HOST-VERIFY PASS','TRADE-MATRIX PASS 6525','TRADE-CANCEL-VERIFY PASS',
                   'TRADE-DRAFT-CHILDREN PASS','TRADE-SYNC-VERIFY PASS','TRADE-QUIRK-VERIFY PASS 4',
                   'TRADE-TRANSFER-VERIFY PASS','TRADE-RECORD-VERIFY PASS',
                   'TRADE-SAVE-FAILED','TRADE-COMMIT','ROSTER-RESET-COMMIT'):
        if marker not in ' '.join(output.split()):raise ValueError('missing host evidence: '+marker)
    # Independent source oracle: 8003D930 latch + 80055314 cancel override.
    # Inspect actual host dispatch logs, not a constant PASS banner.
    required={(1,1):3,(1,2):4,(2,8):2,(2,4):1,(3,2048):6,(4,2048):6,
              (5,256):10,(10,16):6,(11,64):6}
    seen=set()
    for event,raw,cue in re.findall(r'TRADE-CUE\s+event=(\d+) raw=(\d+) cue=(\d+)',output):
        key=(int(event),int(raw));cue=int(cue)
        if cue!=required.get(key,0):raise ValueError('wrong Trade selector cue: '+str((key,cue)))
        seen.add(key)
    if not required.keys()<=seen:raise ValueError('missing Trade selector cue coverage')
    outcomes=re.findall(r'TRADE-MATRIX-OUTCOMES\s+swap=(\d+) transfer=(\d+) both-empty=(\d+) minimum=(\d+);',output)
    if len(outcomes)!=1:raise ValueError('missing/duplicate matrix outcome evidence')
    counts=dict(zip(('swap','transfer','both_empty','minimum'),map(int,outcomes[0])))
    if sum(counts.values())!=6525 or not counts['swap'] or not counts['transfer']:
        raise ValueError('incomplete matrix outcome coverage')
    return counts

def validate(output,directory):
    actual=[line.split('TRADE-CHECKPOINT',1)[1].strip() for line in output.splitlines() if 'TRADE-CHECKPOINT' in line]
    if actual!=CHECKPOINTS:raise ValueError('missing/reordered/duplicate Trade checkpoint')
    validate_summary(output)
    images={}
    for name in CHECKPOINTS:
        path=directory/(name+'.ppm')
        with Image.open(path) as im:
            if im.size!=(512,240) or im.mode!='RGB':raise ValueError('wrong framebuffer')
            im.resize((768,576),Image.Resampling.NEAREST).save(path.with_suffix('.png'))
        images[name]=digest(path)
    if images['entry']==images['second'] or images['compare']==images['compare-layer']:
        raise ValueError('selection/layer frames did not change')
    if images['empty-view-notice']==images['empty-compare-notice']:
        raise ValueError('empty action warning substitution missing')
    return images

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe',type=Path,default=ROOT/'build-windows/Debug/nba97_boot_decomp.exe')
    a=p.parse_args();exe=a.exe.resolve(strict=True)
    parent=ROOT/'.local/verification/trade';parent.mkdir(parents=True,exist_ok=True)
    reserved=Path(tempfile.mkdtemp(prefix='run-',dir=parent));directory=reserved/'host'
    trace=reserved/'trace.log'
    run=subprocess.run([str(exe),'--capture-trade',str(directory),'--trace',str(trace)],cwd=ROOT,capture_output=True,text=True,timeout=90)
    (reserved/'stdout.log').write_text(run.stdout+run.stderr,encoding='utf-8')
    if run.returncode:raise ValueError('Trade host failed: '+(run.stdout+run.stderr)[-2000:])
    images=validate(run.stdout,directory)
    sources=list((ROOT/'src').rglob('*.cpp'))+list((ROOT/'src').rglob('*.c'))+list((ROOT/'src').rglob('*.h'))+list((ROOT/'src').rglob('*.hpp'))
    sources+=[Path(__file__),ROOT/'tools/extract_trade_assets.py']
    report={'schema_version':1,'status':'passed','executed_utc':datetime.now(timezone.utc).isoformat(),
            'checkpoint_count':len(images),'real_database_slot_pairs':6525,'matrix_outcomes':validate_summary(run.stdout),'directory':str(reserved),
            'executable_sha256':digest(exe),'source_sha256':{str(s.relative_to(ROOT)):digest(s) for s in sorted(sources)},
            'frames_sha256':images,'trace_sha256':digest(trace),
            'limits':['Not a completion percentage or MIPS differential proof.','Adjacent-team sample, not all29x28 team combinations.',
                      'Checkpoint rendering and host handlers; not Windows key injection or matched no$psx audio/video timing.']}
    for name in ('trade/ui.n97trade','trade/help.n97ui','database/roster.n97db'):
        path=ROOT/'.local/assetpacks'/name
        if path.exists():report.setdefault('private_input_sha256',{})[name]=digest(path)
    report_path=ROOT/'.local/reports/trade_screen_run.json';report_path.parent.mkdir(parents=True,exist_ok=True)
    report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'TRADE HOST PASS {len(images)} checkpoints; 6525 real-data slot pairs; {reserved}')
if __name__=='__main__':main()
