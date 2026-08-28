#!/usr/bin/env python3
"""Short private Release -> Reset regression; source/reference parity is separate."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from datetime import datetime, timezone

from verify_reorder_save_host import ROOT, inspect_save, sha


def outcome(log, case):
    matches = re.findall(r'REORDER-SAVE-VERIFY\s+' + re.escape(case) +
                        r' PASS; generation=(\d+); default-different=(\d+); team=(\d+); first=(\d+); second=(\d+)', log)
    if len(matches) != 1 or 'AUDIO-ERROR' in log:
        raise ValueError('missing/duplicate host result or audio dispatch error')
    return dict(zip(('generation', 'different', 'team', 'first', 'second'), map(int, matches[0])))


def release_state(log):
    matches = re.findall(r'RESET-RELEASE-CHECK\s+player=(\d+); owner=(-?\d+); pool-count=(\d+); reset-enabled=(\d+)', log)
    if len(matches) != 1:
        raise ValueError('missing/duplicate Release state')
    return dict(zip(('player', 'owner', 'pool', 'enabled'), map(int, matches[0])))


def check_release_state(actual, baseline, released):
    expected = dict(baseline, owner=29 if released else 3,
                    pool=baseline['pool'] + int(released), enabled=int(released))
    if actual != expected:
        raise ValueError(f'Release state mismatch: {actual} != {expected}')


def main():
    from PIL import Image, ImageChops
    parent = ROOT / '.local/verification/reset'
    parent.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='release-host-', dir=parent))
    save = run_root / 'rosters.n97rst'
    exe = ROOT / 'build-windows/Debug/nba97_boot_decomp.exe'
    output = ROOT / '.local/reports/reset_release_host_run.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    protected = [ROOT / p for p in (
        '.local/saves/rosters/default.n97rst', '.local/saves/rosters/default.n97rst.bak',
        '.local/saves/rosters/default.n97rst.lock', '.local/config/frontend_settings.ini',
        '.local/saves/user_profiles.n97sav', '.local/saves/user_profiles.n97sav.bak')]
    asset_root = ROOT / '.local/assetpacks'
    assets = [asset_root / p for p in (
        'database/roster.n97db', 'reorder/reset.n97ui', 'release/help.n97ui',
        'fonts/ZFONT1.PSH', 'menu/ZCURSOR.VH', 'menu/ZCURSOR.VB')]
    def snapshot():
        return {str(p): sha(p) if p.exists() else None for p in protected}
    before = snapshot()
    source_hashes = {str(p): sha(p) for p in assets}
    report = dict(status='running', run_root=str(run_root), cases=[],
                  executed_at_utc=datetime.now(timezone.utc).isoformat(),
                  scope='normal roster, one original player, no created-player catalogue',
                  original_timing_audio_visual_parity='not_verified')
    output.write_text(json.dumps(report, indent=2) + '\n')
    try:
        def run(case, label=None):
            label = label or case
            frames = run_root / label
            result = subprocess.run([str(exe), '--verify-reorder-save', case,
                '--roster-save', str(save), '--capture-reorder-save', str(frames),
                '--trace', str(run_root / (label + '.log'))], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            if result.returncode:
                raise AssertionError(label + ': ' + result.stderr + '\n' + result.stdout[-5000:])
            entry = outcome(result.stdout, case)
            entry.update(case=label, frames={})
            for path in frames.glob('*.ppm'):
                with Image.open(path) as frame:
                    assert frame.size == (512, 240), 'wrong compositor dimensions'
                    frame.save(path.with_suffix('.png'))
                entry['frames'][path.stem] = sha(path)
            assert entry['frames'], 'no compositor evidence'
            if case in ('reset-cancel', 'reset-confirm'):
                with Image.open(frames/'focused.ppm') as focused, Image.open(frames/'held-cancel.ppm') as modal:
                    bounds = ImageChops.difference(focused.convert('RGB'), modal.convert('RGB')).getbbox()
                    assert bounds == (121, 75, 391, 185), ('Reset modal bounds', bounds)
                    assert modal.getpixel((122, 76))[0] > modal.getpixel((122, 76))[1], 'modal is not red'
                cues = [int(v) for v in re.findall(r'role=reset-confirm-[\w-]+ FUN_8002F124 id=(\d+)', result.stdout)]
                assert cues == ([12, 6, 8] if case == 'reset-cancel' else [12, 3, 6, 8]), cues
                entry.update(modal_bounds=bounds, dispatched_sound_ids=cues)
            report['cases'].append(entry)
            print('RESET-RELEASE PASS', label, 'generation=' + str(entry['generation']))
            return entry, result.stdout

        initial, log = run('release-probe', 'baseline')
        baseline = release_state(log)
        assert initial['generation'] == 0 and initial['different'] == 0
        assert baseline['owner'] == 3 and baseline['enabled'] == 0
        assert not save.exists(), 'read-only baseline created save'
        seeded, log = run('release-seed')
        check_release_state(release_state(log), baseline, True)
        assert seeded['generation'] == 1 and seeded['different'] == 1
        accepted = save.read_bytes()
        accepted_time = save.stat().st_mtime_ns
        assert inspect_save(save)['generation'] == 1
        loaded, log = run('release-probe', 'released-restart')
        check_release_state(release_state(log), baseline, True)
        assert loaded['generation'] == 1 and loaded['different'] == 1
        cancelled, log = run('reset-cancel')
        assert cancelled['generation'] == 1 and cancelled['different'] == 1
        assert 'ROSTER-RESET-COMMIT' not in log and '; selected=3;' in log
        loaded, log = run('release-probe', 'cancel-restart')
        check_release_state(release_state(log), baseline, True)
        assert loaded['generation'] == 1 and loaded['different'] == 1
        assert save.read_bytes() == accepted and save.stat().st_mtime_ns == accepted_time, 'Cancel/probe rewrote save'
        restored, log = run('reset-confirm')
        assert restored['generation'] == 2 and restored['different'] == 0
        assert log.count('ROSTER-RESET-COMMIT') == 1 and '; selected=4;' in log
        wire = inspect_save(save)
        assert wire['bytes'] == 68 and wire['generation'] == 2, 'Reset must persist empty override'
        assert Path(str(save) + '.bak').read_bytes() == accepted, 'Reset lost pre-reset backup'
        reset_bytes, reset_time = save.read_bytes(), save.stat().st_mtime_ns
        loaded, log = run('release-probe', 'restored-restart')
        check_release_state(release_state(log), baseline, False)
        assert loaded['generation'] == 2 and loaded['different'] == 0
        assert (loaded['first'], loaded['second']) == (initial['first'], initial['second'])
        locked, _ = run('reset-locked')
        assert locked['generation'] == 2 and locked['different'] == 0
        assert save.read_bytes() == reset_bytes and save.stat().st_mtime_ns == reset_time
        assert not list(run_root.rglob('*.tmp-*')), 'temporary save leaked'
        report.update(status='passed', baseline=baseline, final_save=wire,
                      executable_sha256=sha(exe), source_hashes=source_hashes,
                      implementation_sha256={p: sha(ROOT/p) for p in (
                          'src/win32_main.cpp', 'src/main_menu.cpp', 'src/recovered/roster_reset.c',
                          'src/recovered/roster_trade.c', 'src/roster_save_store.cpp',
                          'tools/verify_reset_release_host.py')})
    except Exception as error:
        report.update(status='failed', error=str(error))
        raise
    finally:
        report['active_files_unchanged'] = snapshot() == before
        report['source_assets_unchanged'] = {str(p): sha(p) for p in assets} == source_hashes
        if not report['active_files_unchanged'] or not report['source_assets_unchanged']:
            report.update(status='failed', error='protected files changed')
        output.write_text(json.dumps(report, indent=2) + '\n')
    assert report['status'] == 'passed', report
    print('RESET-RELEASE SUMMARY 8 fresh-process cases; cancellation, restoration, restart, focus and relock; private assets/saves preserved')


if __name__ == '__main__':
    main()
