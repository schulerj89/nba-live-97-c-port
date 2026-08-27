#!/usr/bin/env python3
"""Local-only construction proof, not an original-screen similarity score."""
import hashlib
import json
import re
from pathlib import Path
import struct
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    from PIL import Image
    output = ROOT / '.local/reports/reorder_screen_run.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running', 'original_visual_parity': 'not_verified',
                'executed_at_utc': datetime.now(timezone.utc).isoformat()}
    output.write_text(json.dumps(evidence, indent=2)+'\n')
    try:
        # Source evidence independently checks graphics state vs input layout.
        overlay = ROOT / '.local/extracted/FEONLY.BIN'
        data = overlay.read_bytes()
        base = 0x80015000
        start, = struct.unpack_from('<I', data, 0x80093330-base+12*4)
        end, = struct.unpack_from('<I', data, 0x800933E0-base+12*4)
        assert end-start == 22*16, 'original graphics record count changed'
        records = []
        for offset in range(start-base, end-base, 16):
            y,x,enabled,depth = struct.unpack_from('<hhhh',data,offset)
            records.append((data[offset+8:offset+12].decode('ascii'),x,y,enabled,depth))
        assert records[5] == ('ba22',156,10,1,3), 'wrong original Re-order title/layout'
        assert records[16:20] == [('frml',30,15,1,1),('frmr',368,15,1,1),
                                 ('dflt',54,22,0,2),('dflt',386,22,0,2)]
        exe = ROOT / 'build-windows/Debug/nba97_boot_decomp.exe'
        database = ROOT / '.local/assetpacks/database/roster.n97db'
        before = sha(database)
        captures = ROOT / '.local/verification/reorder/screen'
        result = subprocess.run([str(exe),'--capture-reorder',str(captures),
            '--trace',str(ROOT / '.local/logs/reorder_capture.log')],cwd=ROOT,
            capture_output=True,text=True,timeout=60)
        print(result.stdout)
        if result.returncode:
            raise RuntimeError(result.stderr or 'native compositor failed')
        assert sha(database)==before, 'capture changed original database'
        assert 'REORDER-ENTRY' in result.stdout and 'graphics=0x0C ba22=(156,10)' in result.stdout
        names=['entry','replacement-scrolled','swapped','discard-prompt']
        hashes={}
        for name in names:
            assert name+'.ppm team=' in result.stdout, 'missing state trace'
            path=captures/(name+'.ppm')
            image=Image.open(path)
            assert image.size==(512,240), 'wrong framebuffer dimensions'
            image.save(path.with_suffix('.png'))
            hashes[name]=sha(path)
        assert len(set(hashes.values()))==4, 'captured states did not change'
        # Real host consumes the shared helpers, not just a passing standalone
        # C test. Swap refreshes both six-row windows exactly once.
        state_counts = {}
        for name in names:
            match = re.search(re.escape(name)+r'\.ppm team=.*?row-revision=(\d+) visible-redraws=(\d+) present-requests=(\d+)', result.stdout)
            assert match, 'missing shared-helper host diagnostics'
            state_counts[name] = tuple(map(int, match.groups()))
        before_counts = state_counts['replacement-scrolled']
        after_counts = state_counts['swapped']
        assert after_counts == (before_counts[0]+1, before_counts[1]+12, before_counts[2]+1), 'swap did not refresh both visible lists once'
        assert state_counts['discard-prompt'] == after_counts, 'opening confirmation unexpectedly refreshed list data'
        assets=['database/roster.n97db','fonts/ZFONT0.PSH','fonts/ZFONT1.PSH',
                'menu/ZSET4.PSP','menu/ZTMPAL.PSH','menu/Z2PORT.IDX','menu/Z2PORT.BIG',
                'reorder/dialogs.n97ui','reorder/discard.n97ui']
        evidence.update(status='passed',graphics_state=12,input_layout=13,
            original_layout_address=hex(start),layout_records=len(records),
            capture_sha256=hashes,shared_helper_counters=state_counts,database_unchanged=True,executable_sha256=sha(exe),
            original_overlay_sha256=sha(overlay),
            asset_sha256={name:sha(ROOT/'.local/assetpacks'/name) for name in assets},
            source_sha256={name:sha(ROOT/name) for name in ['src/win32_main.cpp','src/main_menu.cpp',
                'src/reorder_preview.cpp','src/recovered/reorder_screen.c','src/recovered/roster_reorder.c',
                'src/recovered/roster_lists.c','src/recovered/roster_lists.h',
                'tools/decode_reorder_portraits.py','tools/verify_reorder_screen.py']})
    except Exception as error:
        evidence.update(status='failed',error=str(error))
        raise
    finally:
        output.write_text(json.dumps(evidence,indent=2)+'\n')
    print('REORDER SCREEN PASS: original layout checked; four native frames; source/database/assets hashed.')
    print('No visual similarity percentage, audio proof, or full-feature completion inferred.')


if __name__=='__main__':
    main()
