"""Private Release integration checkpoints; not original-game equivalence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECKPOINTS = '''entry donor-row donor-team donor-bottom help-growing help-open help-return
view view-help view-browsed view-return compare compare-browsed compare-help compare-return
release-start released discard-prompt discard-kept discard-return reentry cancel-return
save-failed accept-return restart minimum-refused quirk-view quirk-return retained-restart reset-return'''.split()
CUES = {(1,1):3, (1,2):4, (2,8):2, (2,4):1, (4,2048):6, (10,16):6, (11,64):6}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_summary(output):
    actual = re.findall(r'RELEASE-CHECKPOINT\s+([^\r\n]+)', output)
    if actual != CHECKPOINTS:
        raise ValueError('missing/reordered/duplicate Release checkpoint')
    for marker in ('RELEASE-HOST-VERIFY', 'PASS 30 checkpoints', 'isolated save only',
                   'ba23=(140,10)', 'state=0x11 descriptor=0x800B152C',
                   'rect=121,80,270,125', 'second NULL', 'RELEASE-SAVE-FAILED',
                   'RELEASE-COMMIT', 'ROSTER-RESET-COMMIT', 'RELEASE-MUTATE',
                   'PASS 435 real-data donor slots', 'reason=retained-pre-child-checkpoint'):
        if marker not in output:
            raise ValueError('missing Release host evidence: ' + marker)
    seen = set()
    for event, raw, cue in re.findall(r'RELEASE-CUE\s+event=(\d+) raw=(\d+) cue=(\d+)', output):
        key = (int(event), int(raw))
        if int(cue) != CUES.get(key, 0):
            raise ValueError('wrong Release selector cue')
        seen.add(key)
    if not CUES.keys() <= seen:
        raise ValueError('missing Release selector cue coverage')
    if output.count('RELEASE-SAVE-FAILED') != 1 or 'TRADE-MUTATION-BLOCKED' in output or 'RELEASE-PENDING' in output:
        raise ValueError('unexpected failure or development gate')


def validate_frames(directory):
    from PIL import Image
    hashes = {}
    for name in CHECKPOINTS:
        path = directory/(name+'.ppm')
        with Image.open(path) as image:
            if image.size != (512, 240) or image.mode != 'RGB':
                raise ValueError('invalid Release framebuffer')
            image.resize((768, 576), Image.Resampling.NEAREST).save(path.with_suffix('.png'))
        hashes[name] = digest(path)
    for first, second in (('entry', 'donor-row'), ('donor-row', 'donor-bottom'),
                          ('help-growing', 'help-open'), ('help-open', 'help-return'),
                          ('entry', 'cancel-return'), ('released', 'discard-prompt'),
                          ('restart', 'minimum-refused'), ('view', 'compare')):
        if hashes[first] == hashes[second]:
            raise ValueError('unchanged required frame: '+first+'/'+second)
    return hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'build-windows/Debug/nba97_boot_decomp.exe')
    args = parser.parse_args()
    exe = args.exe.resolve(strict=True)
    parent = ROOT/'.local/verification/release'
    parent.mkdir(parents=True, exist_ok=True)
    reserved = Path(tempfile.mkdtemp(prefix='run-', dir=parent))
    directory, trace = reserved/'host', reserved/'trace.log'
    run = subprocess.run([str(exe), '--capture-release', str(directory), '--trace', str(trace)],
                         cwd=ROOT, capture_output=True, text=True, timeout=60)
    (reserved/'stdout.log').write_text(run.stdout+run.stderr, encoding='utf-8')
    if run.returncode:
        raise ValueError('Release host failed: '+(run.stdout+run.stderr)[-3000:])
    validate_summary(run.stdout)
    frames = validate_frames(directory)
    sources = [p for p in (ROOT/'src').rglob('*') if p.suffix in ('.c','.cpp','.h','.hpp')]
    sources += [Path(__file__), ROOT/'tools/extract_release_assets.py', ROOT/'tools/extract_trade_assets.py']
    report = {'schema_version': 1, 'status': 'passed', 'executed_utc': datetime.now(timezone.utc).isoformat(),
              'directory': str(reserved), 'checkpoint_count': len(frames), 'frames_sha256': frames,
              'executable_sha256': digest(exe), 'trace_sha256': digest(trace),
              'source_sha256': {str(p.relative_to(ROOT)): digest(p) for p in sources},
              'private_input_sha256': {name: digest(ROOT/'.local/assetpacks'/name) for name in
                  ('release/ui.n97trade','release/help.n97ui','database/roster.n97db',
                   'fonts/ZFONT0.PSH','fonts/ZFONT1.PSH','menu/ZSET4-decoded/ba23.png',
                   'menu/ZSET4-decoded/help.png','menu/ZSET4-team-backgrounds/indexed.n97pal')},
              'limits': ['Native handler and framebuffer capture, not Windows key injection or original execution.',
                         'Original Compare and View/Cancel/reopen retention, ordinary discard and accept/reopen were manually confirmed in separate checkpoints; these are session observations, not original memory-card persistence. This native harness does not itself execute the original.',
                         'Original transition, waveform and animation cadence not yet measured.']}
    path = ROOT/'.local/reports/release_screen_run.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'RELEASE HOST PASS {len(frames)} checkpoints; isolated save/restart/Reset; {reserved}')


if __name__ == '__main__':
    main()
