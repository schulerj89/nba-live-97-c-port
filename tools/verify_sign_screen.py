"""Fresh private Sign host regression; not original-execution equivalence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CHECKPOINTS = '''entry help-first destination-scan view-free-agent view-next-free-agent
view-help compare-free-agent second help-second second-view-browsed second-view-return
second-compare-browsed second-compare-return occupied-refused empty-tail
empty-source-refused vacancy-selected signed save-failed restart second-signing discard
quirk-signed quirk-view-alston quirk-view-return quirk-retained-restart'''.split()
CUES = {(1, 1): 3, (1, 2): 4, (2, 8): 2, (2, 4): 1, (3, 2048): 6,
        (4, 2048): 6, (5, 256): 10, (10, 16): 6, (11, 64): 6}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_summary(output):
    actual = re.findall(r'SIGN-CHECKPOINT\s+([^\r\n]+)', output)
    if actual != CHECKPOINTS:
        raise ValueError('missing/reordered/duplicate Sign checkpoint')
    for marker in ('SIGN-HOST-VERIFY PASS 26 checkpoints:', 'SIGN-SAVE-FAILED',
                   'SIGN-COMMIT', 'ROSTER-RESET-COMMIT'):
        if marker not in ' '.join(output.split()):
            raise ValueError('missing host evidence: ' + marker)
    seen = set()
    for event, raw, cue in re.findall(r'SIGN-CUE\s+event=(\d+) raw=(\d+) cue=(\d+)', output):
        key = (int(event), int(raw))
        if int(cue) != CUES.get(key, 0):
            raise ValueError('wrong Sign selector cue: ' + str((key, cue)))
        seen.add(key)
    if not CUES.keys() <= seen:
        raise ValueError('missing Sign selector cue coverage')
    strips = re.findall(r'SIGN-STRIP-VERIFY\s+xfrZ 39x156 at296,35; original opaque pixels=(\d+)', output)
    if len(strips) != 3 or any(int(n) <= 1000 for n in strips):
        raise ValueError('missing/insufficient free-agent strip pixel evidence')


def validate(output, directory):
    validate_summary(output)
    images = {}
    for name in CHECKPOINTS:
        path = directory / (name + '.ppm')
        with Image.open(path) as im:
            if im.size != (512, 240) or im.mode != 'RGB':
                raise ValueError('wrong Sign framebuffer')
            im.resize((768, 576), Image.Resampling.NEAREST).save(path.with_suffix('.png'))
        images[name] = digest(path)
    for a, b in (('entry', 'help-first'), ('second', 'help-second'),
                 ('second', 'occupied-refused'), ('empty-tail', 'empty-source-refused'),
                 ('vacancy-selected', 'signed'), ('signed', 'save-failed')):
        if images[a] == images[b]:
            raise ValueError('unchanged frame for ' + a + '/' + b)
    return images


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe', type=Path, default=ROOT/'build-windows/Debug/nba97_boot_decomp.exe')
    a = p.parse_args()
    exe = a.exe.resolve(strict=True)
    parent = ROOT/'.local/verification/sign'
    parent.mkdir(parents=True, exist_ok=True)
    reserved = Path(tempfile.mkdtemp(prefix='run-', dir=parent))
    directory, trace = reserved/'host', reserved/'trace.log'
    run = subprocess.run([str(exe), '--capture-sign', str(directory), '--trace', str(trace)],
                         cwd=ROOT, capture_output=True, text=True, timeout=90)
    (reserved/'stdout.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    if run.returncode:
        raise ValueError('Sign host failed: ' + (run.stdout + run.stderr)[-2500:])
    frames = validate(run.stdout, directory)
    sources = sorted(p for p in (ROOT/'src').rglob('*') if p.suffix in ('.c', '.cpp', '.h', '.hpp'))
    sources += [Path(__file__), ROOT/'tools/extract_trade_assets.py']
    report = {'schema_version': 1, 'status': 'passed',
              'executed_utc': datetime.now(timezone.utc).isoformat(),
              'checkpoint_count': len(frames), 'directory': str(reserved),
              'executable_sha256': digest(exe), 'trace_sha256': digest(trace),
              'source_sha256': {str(p.relative_to(ROOT)): digest(p) for p in sources},
              'frames_sha256': frames,
              'private_input_sha256': {n: digest(ROOT/'.local/assetpacks'/n) for n in
                  ('sign/ui.n97trade', 'sign/help.n97ui', 'database/roster.n97db',
                   'menu/ZSET8-decoded/xfrZ.png')},
              'limits': ['Host handlers and rendered checkpoints, not Windows key injection.',
                         'Cue IDs are checked, not waveform/audio-timing equivalence.',
                         'No screenshot similarity percentage or MIPS equivalence claim.',
                         'Synthetic 43,500-pair coverage is in the separate core test, not this host run.']}
    path = ROOT/'.local/reports/sign_screen_run.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'SIGN HOST PASS {len(frames)} checkpoints; isolated save/restart/Reset; {reserved}')


if __name__ == '__main__':
    main()
