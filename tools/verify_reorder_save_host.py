#!/usr/bin/env python3
"""Private host-key save/reload/error proof; not an original fidelity score."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib
import wave
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect_save(path):
    data = path.read_bytes()
    assert data[:8] == b'N97ROST\0'
    assert struct.unpack_from('<H', data, 8)[0] == 1
    assert struct.unpack_from('<I', data, 16)[0] == len(data)
    assert struct.unpack_from('<I', data, len(data)-4)[0] == zlib.crc32(data[:-4])
    return {'generation': struct.unpack_from('<Q', data, 24)[0], 'bytes': len(data), 'sha256': sha(path)}


def main():
    from PIL import Image, ImageChops
    parent = ROOT/'.local/verification/reorder'
    parent.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='save-host-', dir=parent))
    output = ROOT/'.local/reports/reorder_save_host_run.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    exe = ROOT/'build-windows/Debug/nba97_boot_decomp.exe'
    assets = [ROOT/'.local/assetpacks/database/roster.n97db',
              ROOT/'.local/assetpacks/reorder/help.n97ui',
              ROOT/'.local/assetpacks/reorder/compare.n97ui']
    assets.append(ROOT/'.local/assetpacks/reorder/reset.n97ui')
    assets.extend(ROOT/'.local/assetpacks'/name for name in
                  ('fonts/ZFONT1.PSH','menu/ZCURSOR.VH','menu/ZCURSOR.VB'))
    originals = {str(p): sha(p) for p in assets}
    active = ROOT/'.local/saves/rosters/default.n97rst'
    active_files = [active, Path(str(active)+'.bak'), Path(str(active)+'.lock')]
    active_files.extend(ROOT/p for p in ('.local/config/frontend_settings.ini',
                        '.local/saves/user_profiles.n97sav','.local/saves/user_profiles.n97sav.bak'))
    active_before = {str(p): sha(p) if p.exists() else None for p in active_files}
    report = {'status': 'running', 'original_fidelity': 'not_verified', 'run_root': str(run_root),
              'executed_at_utc': datetime.now(timezone.utc).isoformat(), 'cases': []}
    output.write_text(json.dumps(report, indent=2)+'\n')
    try:
        def run(case, save, label=None):
            label = label or case
            frames = run_root/label
            result = subprocess.run([str(exe), '--verify-reorder-save', case,
                '--roster-save', str(save), '--capture-reorder-save', str(frames),
                '--trace', str(run_root/(label+'.log'))], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            if result.returncode:
                raise AssertionError(label+': '+result.stderr+'\n'+result.stdout[-5000:])
            match = re.search(r'REORDER-SAVE-VERIFY\s+'+re.escape(case)+r' PASS; generation=(\d+); default-different=(\d+); team=(\d+); first=(\d+); second=(\d+)', result.stdout)
            assert match, label+' missing host outcome'
            values = [int(v) for v in match.groups()]
            evidence = dict(zip(('generation', 'different', 'team', 'first', 'second'), values))
            evidence['case'] = label
            evidence['frames'] = {}
            for p in frames.glob('*.ppm'):
                image = Image.open(p).convert('RGB')
                assert image.size == (512, 240)
                image.save(p.with_suffix('.png'))
                evidence['frames'][p.stem] = sha(p)
            assert evidence['frames'], 'no host compositor captures'
            if case in ('failure', 'failure-cancel', 'blocked'):
                before = Image.open(frames/'draft.ppm').convert('RGB')
                after = Image.open(frames/'after-notice.ppm').convert('RGB')
                assert ImageChops.difference(before, after).getbbox() is None, 'notice lost frozen draft frame'
                notice = Image.open(frames/'failure-notice.ppm').convert('RGB')
                bounds = ImageChops.difference(before, notice).getbbox()
                assert bounds == (116, 70, 396, 170), ('notice bounds', bounds)
                assert notice.getpixel((117, 71))[1] > notice.getpixel((117, 71))[0], 'notice is not green'
                evidence['notice_bounds'] = bounds
            if case.startswith('reset-') and case!='reset-locked':
                focused=Image.open(frames/'focused.ppm').convert('RGB')
                modal=Image.open(frames/'held-cancel.ppm').convert('RGB')
                bounds=ImageChops.difference(focused,modal).getbbox()
                assert bounds==(121,75,391,185), ('Reset bounds',bounds)
                assert modal.getpixel((122,76))[0]>modal.getpixel((122,76))[1], 'Reset warning is not red'
                evidence['confirmation_bounds']=bounds
                sounds=[int(v) for v in re.findall(r'role=reset-confirm-[\w-]+ FUN_8002F124 id=(\d+)',result.stdout)]
                assert sounds==([12,6,8] if case=='reset-cancel' else [12,3,6,8]), sounds
                evidence['confirmation_sound_ids']=sounds
                evidence['waveform_parity']='not_verified'
                with wave.open(str(frames/'reset-confirm.wav'),'rb') as wav:
                    assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate(),wav.getnframes())==(1,2,22050,8064)
                    pcm=wav.readframes(wav.getnframes())
                    assert any(pcm), 'silent Reset sound export'
                    evidence['confirmation_wav_sha256']=sha(frames/'reset-confirm.wav')
                assert not (frames/'invalid.wav').exists(), 'unpopulated BNKl slot exported'
                assert 'AUDIO-ERROR' not in result.stdout, 'host audio dispatch failed'
            report['cases'].append(evidence)
            print('SAVE-HOST PASS', label, 'generation='+str(evidence['generation']))
            return evidence, result.stdout

        main_save = run_root/'main.n97rst'
        saved, _ = run('save', main_save)
        wire = inspect_save(main_save)
        assert wire['generation'] == 1 and wire['bytes'] == 148 and saved['different'] == 1
        loaded, _ = run('reload', main_save)
        assert (loaded['first'], loaded['second'], loaded['team']) == (saved['first'], saved['second'], saved['team'])
        assert loaded['different'] == 1
        before = main_save.read_bytes(), main_save.stat().st_mtime_ns
        run('noop', main_save, 'saved-noop')
        run('cancel', main_save, 'saved-cancel')
        assert (main_save.read_bytes(), main_save.stat().st_mtime_ns) == before, 'no-op/cancel rewrote accepted file'
        for case in ('noop', 'cancel', 'replacement', 'failure-cancel'):
            path = run_root/(case+'.n97rst')
            run(case, path, 'fresh-'+case)
            assert not path.exists() and not Path(str(path)+'.bak').exists(), 'cancel/no-op created roster save'
        failure, _ = run('failure', run_root/'failure.n97rst')
        assert failure['generation'] == 1 and inspect_save(run_root/'failure.n97rst')['generation'] == 1
        warning, log = run('postcommit', run_root/'warning.n97rst')
        assert warning['generation'] == 1 and log.count('REORDER-COMMIT') == 1, 'postcommit warning replayed edit'
        children, _ = run('children', run_root/'children.n97rst')
        assert children['generation'] == 1 and 'view-help' in children['frames'] and 'compare-help' in children['frames']
        child_reload,_=run('reload',run_root/'children.n97rst','children-reloaded')
        assert child_reload['different']==1 and child_reload['first']==children['first']
        run('reset-confirm',run_root/'children.n97rst','children-reset')
        child_reset_reload,_=run('reload',run_root/'children.n97rst','children-reset-reloaded')
        assert child_reset_reload['different']==0 and child_reset_reload['generation']==2

        # A second swap restores factory order but must write a new valid empty
        # override, not delete the primary and resurrect its backup.
        restored, _ = run('save', main_save, 'restored-order')
        assert restored['different'] == 0 and inspect_save(main_save)['bytes'] == 68
        backup = Path(str(main_save)+'.bak').read_bytes()
        main_save.write_bytes(b'broken fixture')
        repaired, log = run('repair', main_save)
        assert 'backup (primary invalid)' in log and repaired['generation'] == 2 and repaired['different'] == 1
        assert Path(str(main_save)+'.bak').read_bytes() == backup, 'corrupt primary replaced good backup'
        assert inspect_save(main_save)['bytes'] == 148

        for kind in ('future', 'wrong-base'):
            path = run_root/(kind+'.n97rst')
            data = bytearray(backup)
            if kind == 'future':
                struct.pack_into('<H', data, 8, 2)
            else:
                data[32] ^= 1
            struct.pack_into('<I', data, len(data)-4, zlib.crc32(data[:-4]))
            path.write_bytes(data)
            Path(str(path)+'.bak').write_bytes(backup)
            _, log = run('blocked', path, kind)
            assert 'ROSTER-SAVE-BLOCKED' in log and path.read_bytes() == data
            assert Path(str(path)+'.bak').read_bytes() == backup
        for case in ('reset-cancel', 'reset-confirm', 'reset-failure', 'reset-postcommit'):
            path=run_root/(case+'.n97rst')
            path.write_bytes(backup)
            before_reset=path.read_bytes()
            outcome,log=run(case,path)
            if case in ('reset-cancel','reset-failure'):
                assert path.read_bytes()==before_reset and outcome['different']==1
                assert 'ROSTER-RESET-COMMIT' not in log
            else:
                assert outcome['different']==0 and inspect_save(path)['bytes']==68
                assert log.count('ROSTER-RESET-COMMIT')==1
                fresh,_=run('reload',path,case+'-reloaded')
                assert fresh['different']==0 and fresh['generation']==2
                run('reset-locked',path,case+'-relocked')
            if case=='reset-failure':
                # Fresh retry must start from intact accepted state, not a
                # half-published default table.
                run('reset-confirm',path,'reset-failure-retry')
                assert inspect_save(path)['bytes']==68
        run('reset-locked',run_root/'never-saved.n97rst','reset-defaults-locked')
        assert {str(p): sha(p) for p in assets} == originals, 'private source asset changed'
        assert {str(p): sha(p) if p.exists() else None for p in active_files} == active_before, 'active save touched'
        assert not list(run_root.rglob('*.tmp-*')), 'owned temporary file leaked'
        report.update(status='passed', source_hashes=originals, executable_sha256=sha(exe),
                      active_save_unchanged=True, actual_host_key_handlers=True)
        report['implementation_sha256']={p:sha(ROOT/p) for p in (
            'src/win32_main.cpp','src/roster_database.cpp','src/roster_save_store.cpp',
            'src/roster_save_codec.cpp','src/roster_reset_assets.cpp','src/recovered/roster_reset.c',
            'src/recovered/roster_reorder.c','src/recovered/frontend_help.c','src/recovered_audio.cpp',
            'tools/verify_reorder_save_host.py','tools/extract_roster_reset.py')}
    except Exception as error:
        report.update(status='failed', error=str(error))
        raise
    finally:
        output.write_text(json.dumps(report, indent=2)+'\n')
    print('SAVE-HOST SUMMARY', len(report['cases']), 'fresh-process cases; Reset/save UI covered; original fidelity still pending')


if __name__ == '__main__':
    main()
