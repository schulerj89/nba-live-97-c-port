#!/usr/bin/env python3
"""Local-only construction proof, not an original-screen similarity score."""
import hashlib
import json
import re
from pathlib import Path
import struct
import subprocess
import wave
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    from PIL import Image, ImageChops
    from extract_frontend_help import extract as extract_help
    from extract_compare_assets import extract as extract_compare
    from extract_player_notice import extract as extract_notice
    output = ROOT / '.local/reports/reorder_screen_run.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running', 'original_visual_parity': 'not_verified',
                'executed_at_utc': datetime.now(timezone.utc).isoformat()}
    output.write_text(json.dumps(evidence, indent=2)+'\n')
    try:
        # Source evidence independently checks graphics state vs input layout.
        overlay = ROOT / '.local/extracted/FEONLY.BIN'
        data = overlay.read_bytes()
        assert (ROOT/'.local/assetpacks/player/no-facts.n97ui').read_bytes()==extract_notice(data), \
            'no-facts descriptor/continuation prompt differ from original'
        help_file = ROOT / '.local/assetpacks/reorder/help.n97ui'
        assert help_file.read_bytes() == extract_help(data), 'Help pack differs from original pointer-selected descriptors'
        assert (ROOT/'.local/assetpacks/reorder/compare.n97ui').read_bytes()==extract_compare(data), 'Compare text/fields differ from original'
        base = 0x80015000
        scroll_source_blocks=[
            (0x8005a1ec,0x8005a280,'68d0594e912c2b4b07cad4f076418b4e88d1891c69ed29bcaecf0d373acac265'),
            (0x8003d930,0x8003e414,'f4c007cc400f511be6502511119f1f65e27187d2f1380f277f640c6f56644156'),
            (0x8002b1e4,0x8002b34c,'056296bf07bc62008639b9bc1918d1ad5b9a276eedc16a823d054747a80c4bbc'),
            (0x8002b5ac,0x8002b830,'891540d3bd0150267f3fa81da82eb16e6a33919ce41ee32a626277c2ef41cf82'),
            (0x8002ba54,0x8002bb08,'abd9683a0bb623ac63ed54fd4fecba16bca39b61f382f47cb956d6497e52b5cd'),
            (0x8002bba0,0x8002bff0,'d8f97480c11b750ff2ed23299e7b8478896b13322fb84f35870f4918f324b415'),
            (0x8003a650,0x8003a6bc,'d913c40ce9d5b2279052d787dd9f86e6f745bf72b0ecf1678c0ace21beb0beaa'),
            (0x8003a6bc,0x8003ab64,'646bf9b646d4e934c01546518ec392d51153de1a2a997dc701fa61c40c81a774'),
            (0x8003ab64,0x8003ac10,'ad53497aea73c603b75b85a310885d9237d7f571ae2e066daa91b6c92bf59bf6'),
            (0x8003b15c,0x8003b16c,'cde33a270a8974c11afb027e78fa1ecd1b305b14ad4b94d31b64bdd8f0434b69'),
            (0x8002d3b0,0x8002d5bc,'4a77dfdd0138a3541a80dc3b87194d0c4cf0fb946d9f8309c3ce89f025c28ea4')]
        for first,end,digest in scroll_source_blocks:
            assert hashlib.sha256(data[first-base:end-base]).hexdigest()==digest, 'audited scroll control-flow block changed'
        # Static descriptors are NOT their initialized runtime values. 5A1EC
        # loads manager+88 (id0) and+10C (id33), then stores zero to each+20.
        # The second store is in JR's delay slot. 3D930 tests that pointer
        # before assigning cue3, dispatching3AB64 or pumping post-delay3.
        assert [struct.unpack_from('<I',data,a-base)[0] for a in range(0x8005a264,0x8005a280,4)] == [
            0x8cc30088,0x34020038,0xa0c2000b,0xac600020,0x8cc2010c,0x03e00008,0xac400020], \
            'Compare first-row Up callback initialization changed'
        for i in list(range(24))+list(range(33,57)):
            descriptor,=struct.unpack_from('<I',data,0x800a48b4-base+i*4)
            assert struct.unpack_from('<II',data,descriptor-base+32)==(0x8003ab64,0x8003ab64), \
                'Compare static scroll callbacks changed before the two runtime Up overrides'
        # Guard the specialization, including atlas-tagged/transposed controls.
        scroll_font=ROOT/'.local/assetpacks/fonts/ZFONT1.PSH'
        font_bytes=scroll_font.read_bytes()
        heights=[]
        for entry in range(struct.unpack_from('<I',font_bytes,8)[0]):
            tag,at=struct.unpack_from('<4sI',font_bytes,16+8*entry)
            try: int(tag,16)
            except ValueError: continue
            assert font_bytes[at]==0x40
            w,h=struct.unpack_from('<HH',font_bytes,at+4)
            transposed=struct.unpack_from('<h',font_bytes,at+12)[0]<0
            heights.append(w if transposed else h)
        assert heights and min(heights)>0 and max(heights)==14, 'scroll projection requires full glyph collapse at14 pixels'
        # Audited normal-mode repeat path: context+720==0 takes a record pass
        # at3AFD0, publishes controller/mask, then J3B0B0 executes a SECOND
        # record pass. Hash complete branch blocks, not only ADDIU literals;
        # a correct increment with the wrong control-flow edge is not proof.
        repeat_source_blocks=[
            (0x8003aec0,0x8003aed4,'f6b3e3c01e40f33b81ff565014ba7adba9f08eaa05ca425b72cf9c3bedc4d330'),
            (0x8003afd0,0x8003b038,'428798e4f37dba95cb60b861b3874f943f3086dfa9b79e74864711234a712a05'),
            (0x8003b0b0,0x8003b128,'85495e2f3ac76afacfb09be6cc0dd6028a865c61070d132875ed3f62a7287ff6')]
        for first,end,digest in repeat_source_blocks:
            assert hashlib.sha256(data[first-base:end-base]).hexdigest()==digest, \
                f'original repeat-counter control-flow block changed at {first:08X}'
        callback_source_blocks=[
            (0x8003e33c,0x8003e39c,'1c109edce297b432bec679aa12f3cf8329690208a34a1d106fb8a0c430fc13bc'),
            (0x80059f20,0x8005a014,'881c839767245c0e3309ce6eb07cd8518d43fb336182caff3c9cdaa2221ca9f9')]
        for first,end,digest in callback_source_blocks:
            assert hashlib.sha256(data[first-base:end-base]).hexdigest()==digest, \
                f'original generic-callback dispatch/delay block changed at {first:08X}'
        for i in list(range(24))+list(range(33,57)):
            descriptor,=struct.unpack_from('<I',data,0x800a48b4-base+i*4)
            assert struct.unpack_from('<I',data,descriptor-base+48)[0]==0x80059f20, 'Compare row generic callback changed'
        assert struct.unpack_from('<I',data,0x8003e274-base)[0]==0x30823e50, 'generic callback ANDI mask changed'
        speech_gain_words={0x80031808:0x3C028002,0x8003180C:0x90421D7D,
                           0x80031820:0x00021900,0x80031824:0x00623023,0x80031828:0x28C20080}
        assert all(struct.unpack_from('<I',data,address-base)[0]==word
                   for address,word in speech_gain_words.items()), 'original speech setting/scale/clamp words changed'
        player_layout,=struct.unpack_from('<I',data,0x80093330-base+36*4)
        assert player_layout==0x80097A24, 'unexpected original Player layout'
        for index,tag,enabled,depth in [(20,b'o18a',1,3),(21,b'o18b',0,2)]:
            offset=player_layout-base+index*16
            assert struct.unpack_from('<hhhh',data,offset)==(198,356,enabled,depth) and \
                data[offset+8:offset+12]==tag, 'Cool Fact overlay/base source record changed'
        start, = struct.unpack_from('<I', data, 0x80093330-base+12*4)
        end, = struct.unpack_from('<I', data, 0x800933E0-base+12*4)
        assert end-start == 22*16, 'original graphics record count changed'
        records = []
        for offset in range(start-base, end-base, 16):
            y,x,enabled,depth = struct.unpack_from('<hhhh',data,offset)
            records.append((data[offset+8:offset+12].decode('ascii'),x,y,enabled,depth))
        assert records[5] == ('ba22',156,10,1,3), 'wrong original Re-order title/layout'
        for layout,table,tag,x,y in [(12,0x80096bc4,b'ba22',156,10),(16,0x80096794,b'ba35',142,10),
                                    (35,0x800978c4,b'ba02',170,15),(36,0x80097a24,b'ba41',40,18)]:
            assert struct.unpack_from('<I',data,0x80093330-base+layout*4)[0]==table
            record=data[table-base+80:table-base+96]
            assert record==struct.pack('<hhhh4sI',y,x,1,3,tag,0x100), 'title source registration changed'
        assert records[4] == ('help',235,217,1,1), 'original Help object4 layout changed'
        assert data[0x8009B230-base:0x8009B238-base]==b'hel1hel2', 'original Help tag table changed'
        for state,expected_second in [(12,0x800B102C),(35,0),(36,0)]:
            table,=struct.unpack_from('<I',data,0x800B00E0-base+state*4)
            second,=struct.unpack_from('<I',data,table-base+4)
            assert second==expected_second, 'numbered Help footer eligibility changed'
        assert records[16:20] == [('frml',30,15,1,1),('frmr',368,15,1,1),
                                 ('dflt',54,22,0,2),('dflt',386,22,0,2)]
        exe = ROOT / 'build-windows/Debug/nba97_boot_decomp.exe'
        title_exe=ROOT/'build-windows/Debug/nba97_frontend_title_tests.exe'
        title_run=subprocess.run([str(title_exe)],cwd=ROOT,capture_output=True,text=True,timeout=30,check=True)
        title_checks=re.findall(r'^TITLE PASS (.+)$',title_run.stdout,re.MULTILINE)
        assert len(title_checks)==6 and len(set(title_checks))==6, 'incomplete title tests'
        photo_exe=ROOT/'build-windows/Debug/nba97_player_photo_tests.exe'
        photo_run=subprocess.run([str(photo_exe)],cwd=ROOT,capture_output=True,text=True,timeout=30,check=True)
        photo_checks=re.findall(r'^PHOTO PASS (.+)$',photo_run.stdout,re.MULTILINE)
        assert len(photo_checks)==3, 'incomplete photo lifecycle tests'
        evidence['photo_lifecycle_checks']=photo_checks
        evidence['photo_lifecycle_test_sha256']=sha(photo_exe)
        audio_exe=ROOT/'build-windows/Debug/nba97_recovered_audio_tests.exe'
        audio_run=subprocess.run([str(audio_exe),str(ROOT/'.local/assetpacks/menu')],cwd=ROOT,
            capture_output=True,text=True,timeout=30,check=True)
        audio_checks=re.findall(r'^AUDIO PASS (\w+)$',audio_run.stdout,re.MULTILINE)
        assert audio_checks==['volume_256_inputs','speech_256_settings_and_stop_feedback','synthetic_168_cursor_levels_double_floor_gain_and_quantized_pitch',
            'cursor_source_rejections_atomic_info_output_and_mute_callback_guard','synthetic_cool_fact_reserved_record_and_player_boundary',
            'synthetic_speech_24_gain_vectors_and_prepare_isolation','synthetic_speech_truncations_and_invalid_tones',
            'private_144_cue_levels_exact_pcm_and_pitch','private_2465_speech_mappings_and_six_clip_payloads',
            'private_speech_72_gain_vectors'], 'incomplete audio tests'
        evidence['audio_gain_checks']=audio_checks
        evidence['audio_original_waveform_parity']='not_verified'
        fact_exe=ROOT/'build-windows/Debug/nba97_cool_fact_selection_tests.exe'
        fact_run=subprocess.run([str(fact_exe)],cwd=ROOT,capture_output=True,text=True,timeout=30,check=True)
        fact_checks=re.findall(r'^FACT PASS (\w+)$',fact_run.stdout,re.MULTILINE)
        assert fact_checks==['all_masks_previous_values_and_256_draws','all_243_flag_states_and_unused_routes',
            'depletion_refill_and_no_repeat_cycle','single_variant_and_sparse_original_edge',
            'eight_present_overlay_and_deferred_consumption'], 'fact selection tests incomplete'
        notice_exe=ROOT/'build-windows/Debug/nba97_player_notice_tests.exe'
        notice_run=subprocess.run([str(notice_exe),str(ROOT/'.local/assetpacks')],cwd=ROOT,
            capture_output=True,text=True,timeout=30,check=True)
        notice_checks=re.findall(r'^NOTICE PASS (\w+)$',notice_run.stdout,re.MULTILINE)
        assert notice_checks==['descriptor_footer_and_bounds','fourteen_dismiss_controls_and_13_tick_barriers',
            'red_style_not_fullscreen','index_absence_vs_corruption','private_font_rows_105_117_135'], 'notice tests incomplete'
        help_exe = ROOT / 'build-windows/Debug/nba97_frontend_help_tests.exe'
        help_run = subprocess.run([str(help_exe), str(ROOT/'.local/assetpacks')], cwd=ROOT,
            capture_output=True, text=True, timeout=30, check=True)
        help_checks = re.findall(r'^HELP PASS (\w+)$',help_run.stdout,re.MULTILINE)
        assert len(help_checks)==11 and len(set(help_checks))==11 and \
            'invalid_open_leaves_state_untouched' in help_checks, 'incomplete Help native test run'
        database = ROOT / '.local/assetpacks/database/roster.n97db'
        before = sha(database)
        child_exe = ROOT / 'build-windows/Debug/nba97_reorder_child_tests.exe'
        child_run = subprocess.run([str(child_exe),str(database)],cwd=ROOT,
            capture_output=True,text=True,timeout=30,check=True)
        child_checks = re.findall(r'^CHILD PASS (\w+)$',child_run.stdout,re.MULTILINE)
        assert len(child_checks)==27 and len(set(child_checks))==27, 'incomplete child native test run'
        assert {'release_card_availability_navigation', 'synthetic_view_team_scan_338_slot_retention_cases',
                'synthetic_view_layout24_single_group_refresh', 'synthetic_view_empty_team_guard',
                'synthetic_view_all_normal_team_slots_scan_both_directions',
                'private_view_all_normal_team_slots_scan_both_directions'} <= set(child_checks), \
            'missing source-backed View team scan/layout regressions'
        compare_exe = ROOT / 'build-windows/Debug/nba97_roster_compare_tests.exe'
        compare_run = subprocess.run([str(compare_exe)],cwd=ROOT,
            capture_output=True,text=True,timeout=30,check=True)
        compare_checks = re.findall(r'^COMPARE PASS (\w+)$',compare_run.stdout,re.MULTILINE)
        assert len(compare_checks)==18 and len(set(compare_checks))==18 and {'two_present_text_barrier','single_free_agent_suppression_is_not_generic_team_lock','left_right_acceleration_post_delay_and_poll_frame','normal_poll_records_counter_in_branch_and_common_tail','generic_callback_wait_chords_and_silent_noops','scroll_group_order_both_sides_all_layers_and_endpoints','scroll_fixed_wait_held_input_exact_masks','first_row_up_callback_disabled_both_groups_preserves_poll','display_animation_wait_is_not_keyboard_release','rebuilt_selected_text_clears_geometry_wait_flags'} <= set(compare_checks), 'incomplete Compare controller tests'
        assert 'COMPARE NULL-UP polls=320 sides=2 layers=4 callback=none delay=0 poll=1 source=5A1EC/3D930' in compare_run.stdout
        assert 'COMPARE REFRESH sequences=116 state_bytes=14' in compare_run.stdout
        assert 'COMPARE FREE REFRESH sequences=4 counts=100/1 ordinary_single=callback' in compare_run.stdout
        assert 'COMPARE PACING actions=40 post_and_poll_presents=120 state_bytes=4' in compare_run.stdout
        assert 'COMPARE COUNTER normal_record_passes=2 vectors=150 rejected_masks=65534 first=2 repeat_step=4 cap=48' in compare_run.stdout
        assert 'COMPARE CALLBACK masks=65024 delay=5 poll=1 supported_exact_masks=5 silent_noops_wait=yes' in compare_run.stdout
        assert 'COMPARE ANIMATION vectors=16777216 source=2C610' in compare_run.stdout
        pack_exe=ROOT/'build-windows/Debug/nba97_compare_assets_tests.exe'
        pack_run=subprocess.run([str(pack_exe),str(ROOT/'.local/assetpacks/reorder/compare.n97ui')],cwd=ROOT,
            capture_output=True,text=True,timeout=30,check=True)
        pack_checks=re.findall(r'^COMPARE-ASSET PASS (\w+)$',pack_run.stdout,re.MULTILINE)
        assert len(pack_checks)==4 and len(set(pack_checks))==4, 'Compare pack tests incomplete'
        captures = ROOT / '.local/verification/reorder/screen'
        result = subprocess.run([str(exe),'--capture-reorder',str(captures),
            '--trace',str(ROOT / '.local/logs/reorder_capture.log')],cwd=ROOT,
            capture_output=True,text=True,timeout=60)
        print(result.stdout)
        if result.returncode:
            raise RuntimeError(result.stderr or 'native compositor failed')
        # Independent instruction-arithmetic oracle against real host vertices
        # and rendered frames. No original runtime seed/cadence claim.
        title_rows=re.findall(r'TITLE-VERIFY frame=(\d+) rng=(\d+) phase=(\d+) draws=(\d+) xy=([\d,]+)',result.stdout)
        assert len(title_rows)==9, 'missing title presentation sequence'
        title_base=[156,10,358,10,156,64,358,64]
        def title_random(seed):
            seed=seed or 0xa5a5
            return ((seed<<1) ^ (0x1d87 if seed&0x4000 else 0))&65535
        title_frames=[];title_hashes={};title_traces=[]
        for index,row in enumerate(title_rows):
            number,seed,phase,draws=map(int,row[:4]);xy=list(map(int,row[4].split(',')))
            assert number==index and phase in (0,1) and len(xy)==8
            if index==0:
                assert xy==title_base, 'title did not start with source asset rectangle'
            else:
                prior=title_traces[-1];expected_seed=title_random(prior['rng']);expected_xy=prior['xy'][:]
                consumed=1
                if prior['phase']==0:
                    for corner in range(8):
                        expected_seed=title_random(expected_seed);consumed+=1
                        expected_xy[corner]=title_base[corner]+(expected_seed&3)
                assert (seed,phase,draws,xy)==(expected_seed,prior['phase']^1,prior['draws']+consumed,expected_xy), \
                    'host title draw order, global phase or baseline arithmetic differs'
            path=captures/f'title-reorder-{index}.ppm'
            frame=Image.open(path).convert('RGB');assert frame.size==(512,240)
            frame.save(path.with_suffix('.png'));title_frames.append(frame);title_hashes[path.name]=sha(path)
            if index:
                diff=ImageChops.difference(title_frames[-2],frame).getbbox()
                if title_traces[-1]['phase']==1:
                    assert diff is None, 'absent title slot changed rendered pixels'
                elif xy!=title_traces[-1]['xy']:
                    assert diff is not None, 'changed title vertices were not rendered'
                if diff:
                    assert diff[0]>=156 and diff[1]>=10 and diff[2]<=361 and diff[3]<=67, \
                        'title-only presentation changed pixels outside original extent plus3'
            title_traces.append({'frame':number,'rng':seed,'phase':phase,'draws':draws,'xy':xy})
        assert len(set(title_hashes.values()))>=3, 'title sequence never visibly varied'
        evidence['title_motion']={'native_checks':title_checks,'test_executable_sha256':sha(title_exe),
            'host_trace':title_traces,'capture_sha256':title_hashes,'repaint_does_not_advance':True,
            'native_clock_policy':'nominal30Hz, only after preceding frame painted; no catch-up',
            'original_runtime_seed_cadence_and_raster_parity':'not_verified'}
        scroll_begin='COMPARE-SCROLL-VERIFY begin'
        scroll_end='COMPARE-SCROLL-VERIFY end actions=45 cues=40 presentations=251; held/reversal/endpoints; draft unchanged'
        assert result.stdout.count(scroll_begin)==1 and result.stdout.count(scroll_end)==1, 'missing scroll host scenario'
        start=result.stdout.rfind('\n',0,result.stdout.index(scroll_begin))+1
        end=result.stdout.index('\n',result.stdout.index(scroll_end))+1
        scroll_output=result.stdout[start:end]
        scroll_actions=[tuple(map(int,r)) for r in re.findall(
            r'COMPARE-SCROLL-VERIFY action mask=(\d+) top=(\d+) callback=(\d+) post=(\d+) held=(\d+)',scroll_output)]
        expected_scroll=[(1,0,0,1,0),(2,1,2,4,0),(1,0,2,4,0)]
        expected_scroll += [(2,min(i+1,19),2 if i<19 else 0,4,2 if i<20 else 0) for i in range(21)]
        expected_scroll += [(1,max(18-i,0),2 if i<19 else 0,4 if i<19 else 1,1 if i<20 else 0) for i in range(21)]
        assert scroll_actions==expected_scroll, 'scroll repeat/reversal/endpoint sequence differs'
        scroll_sounds=list(map(int,re.findall(r'role=compare-input FUN_8002F124 id=(\d+)',scroll_output)))
        assert scroll_sounds==[4,3]+[4]*19+[3]*19, 'scroll endpoints or blocked keys emitted a cue'
        result.stdout=result.stdout[:start]+result.stdout[end:]
        scroll_frames={}
        scroll_hashes={}
        for direction in ('down','up'):
            frames=[]
            for phase in range(3):
                name=f'compare-scroll-{direction}-{phase}'
                path=captures/(name+'.ppm')
                frame=Image.open(path).convert('RGB');assert frame.size==(512,240)
                frame.save(path.with_suffix('.png'));frames.append(frame);scroll_hashes[name]=sha(path)
            for bounds,left_group in [((80,130,178,198),True),((183,130,326,198),True),((335,130,433,198),False)]:
                crops=[frame.crop(bounds).tobytes() for frame in frames]
                assert (crops[1]==crops[2] and crops[0]!=crops[1]) if left_group else (crops[0]==crops[1] and crops[1]!=crops[2]), \
                    f'{direction}: actual pixels violate group0 then group1 update order'
            scroll_frames[direction]=frames
        for bounds in [(80,130,178,198),(183,130,326,198),(335,130,433,198)]:
            assert scroll_frames['down'][0].crop(bounds).tobytes()==scroll_frames['up'][2].crop(bounds).tobytes(), 'up failed to restore original row geometry'
            assert scroll_frames['down'][2].crop(bounds).tobytes()==scroll_frames['up'][0].crop(bounds).tobytes(), 'scroll row geometry changed without input'
        # Decode the private Down glyph independently of the C++ compositor.
        # Validate the first two color updates and rebuild-to-neutral on Up.
        down_glyphs=[]
        for entry in range(struct.unpack_from('<I',font_bytes,8)[0]):
            tag,at=struct.unpack_from('<4sI',font_bytes,16+8*entry)
            try: code=int(tag,16)&255
            except ValueError: continue
            if code==0x8c: down_glyphs.append(at)
        assert len(down_glyphs)==1
        at=down_glyphs[0]
        w,h=struct.unpack_from('<HH',font_bytes,at+4)
        bearing,position=struct.unpack_from('<hh',font_bytes,at+10)
        palette=at+int.from_bytes(font_bytes[at+1:at+4],'little')
        stride=((w+1)//2+1)&~1
        glyph_pixels=[]
        for sy in range(h):
            for sx in range(w):
                packed=font_bytes[at+16+sy*stride+sx//2]
                index=(packed>>4 if sx&1 else packed&15)
                if not index: continue
                color,=struct.unpack_from('<H',font_bytes,palette+16+index*2)
                rgb=tuple(((color>>shift)&31)*8+((color>>shift)&31)//4 for shift in (0,5,10))
                gx,gy=(sy,sx) if position<0 else (sx,sy)
                glyph_pixels.append((246+gx,203-bearing+gy,rgb))
        assert glyph_pixels
        scroll_arrow_pixels=0
        for direction,phase,tint in [('down',0,(128,128,128)),('down',1,(126,122,96)),
                                     ('down',2,(124,115,64)),('up',1,(128,128,128)),('up',2,(128,128,128))]:
            for x,y,rgb in glyph_pixels:
                assert scroll_frames[direction][phase].getpixel((x,y))==tuple(min(255,c*m//128) for c,m in zip(rgb,tint)), \
                    'vertical marker glyph, fade phase or reset differs'
                scroll_arrow_pixels+=1
        generic_begin='COMPARE-GENERIC-VERIFY begin'
        generic_end='COMPARE-GENERIC-VERIFY end waits=16 presentations=96; five held routes, four silent noops, Help/Cancel; draft unchanged'
        assert result.stdout.count(generic_begin)==1 and result.stdout.count(generic_end)==1, 'missing generic callback host scenario'
        start=result.stdout.rfind('\n',0,result.stdout.index(generic_begin))+1
        end=result.stdout.index('\n',result.stdout.index(generic_end))+1
        generic_output=result.stdout[start:end]
        generic_waits=[tuple(map(int,r)) for r in re.findall(r'COMPARE-GENERIC-VERIFY wait mask=(\d+) held=(\d+) presents=(\d+);',generic_output)]
        generic_expected=[row for mask in (512,1024,4096,8192,2048) for row in ((mask,mask,6),(mask,0,6))]
        generic_expected += [(mask,0,6) for mask in (16,64,1028,1536)]+[(1024,32,6),(8192,256,6)]
        assert generic_waits==generic_expected, 'held/no-op/Help/Cancel callback sequence differs'
        generic_sounds=list(map(int,re.findall(r'role=compare-input FUN_8002F124 id=(\d+)',generic_output)))
        assert generic_sounds==[6]*12, 'generic callbacks: expected12 audible,4 silent, with no blocked-key sounds'
        assert generic_output.count('COMPARE-CALLBACK mask=')==16, 'unexpected extra generic callbacks'
        assert 'completed-present=' not in generic_output, 'generic callback acquired player-cycle text wait'
        result.stdout=result.stdout[:start]+result.stdout[end:]
        # Keep the original85-frame route assertions scoped to that run section.
        # Independently require/check the additional host held-input scenario;
        # do not silently discard its new callback/audio events.
        begin_token='COMPARE-HELD-VERIFY begin'
        end_token='COMPARE-HELD-VERIFY end actions=40 presentations=200; wrap/release/reversal/chord/help/cancel passed; draft unchanged; original timing comparison pending'
        assert result.stdout.count(begin_token)==1 and result.stdout.count(end_token)==1, 'missing complete held host run'
        held_start=result.stdout.rfind('\n',0,result.stdout.index(begin_token))+1
        held_end=result.stdout.index('\n',result.stdout.index(end_token))+1
        held_output=result.stdout[held_start:held_end]
        assert held_output.count('COMPARE-REFRESH completed-present=1 ')==45, 'held/arrow callback first phase count'
        assert held_output.count('COMPARE-REFRESH completed-present=2 ')==45, 'held/arrow callback second phase count'
        held_sounds=list(map(int,re.findall(r'role=compare-input FUN_8002F124 id=(\d+)',held_output)))
        assert held_sounds==[1]*42+[2,1,2], 'held/arrow player cues out of order'
        assert 'held=12 ' in held_output and 'held=32 ' in held_output and 'held=256 ' in held_output
        result.stdout=result.stdout[:held_start]+result.stdout[held_end:]
        arrow_proof=re.search(r'COMPARE-ARROW-VERIFY phases=22 original-glyph-pixels=(\d+) outside-unchanged=yes; native-neutral-start=128; original allocator start not captured',held_output)
        assert arrow_proof and int(arrow_proof[1])>300, 'missing original arrow glyph/full-frame checks'
        arrow_names=[f'compare-arrow-{phase}' for phase in range(22)]
        arrow_hashes={}
        arrow_frames=[]
        for name in arrow_names:
            assert name+'.ppm team=' in held_output, 'missing arrow capture trace'
            path=captures/(name+'.ppm')
            frame=Image.open(path).convert('RGB')
            assert frame.size==(512,240)
            frame.save(path.with_suffix('.png'))
            arrow_frames.append(frame);arrow_hashes[name]=sha(path)
        assert arrow_hashes[arrow_names[0]]==arrow_hashes[arrow_names[20]]==arrow_hashes[arrow_names[21]], 'arrow did not return to neutral'
        assert len({arrow_hashes[n] for n in arrow_names[4:17]})==1, 'arrow hold recolors or flickers'
        assert len(set(arrow_hashes.values()))>=7, 'missing interpolated arrow colors'
        # Derive the footprint from the original glyph metadata, not a guessed
        # screen rectangle. Source29EC0 transposes when Position-X is negative.
        font_data=(ROOT/'.local/assetpacks/fonts/ZFONT1.PSH').read_bytes()
        assert font_data[:4]==b'SHPP' and font_data[12:16]==b'GIMX'
        glyph_records=[]
        for i in range(struct.unpack_from('<I',font_data,8)[0]):
            tag,offset=struct.unpack_from('<4sI',font_data,16+i*8)
            try: code=int(tag,16)
            except ValueError: continue
            if code&255==0x8a: glyph_records.append(offset)
        assert len(glyph_records)==1, 'ambiguous original arrow glyph'
        glyph_at=glyph_records[0]
        assert font_data[glyph_at]==0x40
        glyph_w,glyph_h=struct.unpack_from('<HH',font_data,glyph_at+4)
        bearing,position_x=struct.unpack_from('<hh',font_data,glyph_at+10)
        if position_x<0: glyph_w,glyph_h=glyph_h,glyph_w
        glyph_x=135-(glyph_w-1)//2 # Original font selector1 uses kerning1.
        glyph_y=116-bearing
        arrow_bounds=(glyph_x,glyph_y,glyph_x+glyph_w,glyph_y+glyph_h)
        for frame in arrow_frames[1:20]:
            delta=ImageChops.difference(arrow_frames[0],frame).getbbox()
            assert delta and delta[0]>=arrow_bounds[0] and delta[2]<=arrow_bounds[2] and delta[1]>=arrow_bounds[1] and delta[3]<=arrow_bounds[3], 'flash moved/changed another object'
        tint_exe=ROOT/'build-windows/Debug/nba97_reorder_tests.exe'
        tint_run=subprocess.run([str(tint_exe)],cwd=ROOT,capture_output=True,text=True,timeout=30,check=True)
        assert 'REORDER PASS arrow_flash_256_starts_21_updates_retrigger ' in tint_run.stdout
        assert sha(database)==before, 'capture changed original database'
        assert 'REORDER-ENTRY' in result.stdout and 'graphics=0x0C ba22=(156,10)' in result.stdout
        screen_names=['entry','replacement-scrolled','swapped','discard-prompt']
        help_names=[f'help-{stage}-{phase}' for stage in ('first','replacement')
                    for phase in ('start','open','closing','returned')]
        view_names=[f'view-{stage}-{phase}' for stage in ('first','replacement','swapped')
                    for phase in ('entered','browsed','help','returned','photo-wait','photo-cycle-wait')]
        compare_names=[f'compare-{stage}-{phase}' for stage in ('first','replacement','swapped')
                       for phase in ('parent','entered','side','browsed','help','returned')]
        compare_extra=['compare-ratings','compare-attributes','compare-attributes-bottom','compare-free-agents']
        palette_names=[f'compare-palette-{i}' for i in range(17)]
        refresh_names=[f'compare-refresh-{i}' for i in range(3)]
        notice_names=['no-facts-'+suffix for suffix in ('parent','player','opening','open','closing','returned','editor-return')]
        fact_names=['fact-cycle-'+suffix for suffix in ('parent','player','returned','editor-return')]
        flash_names=[f'fact-flash-{frame}' for frame in range(8)]
        names=screen_names+help_names+view_names+compare_names+compare_extra+notice_names+fact_names+flash_names+palette_names+refresh_names+['compare-palette-settled']
        hashes={}
        for name in names:
            assert name+'.ppm team=' in result.stdout, 'missing state trace'
            path=captures/(name+'.ppm')
            image=Image.open(path)
            # Lossless, viewable proof alongside the compositor's raw PPM.
            # No scaling, filtering or generated replacement pixels.
            image.save(captures/(name+'.png'))
            assert image.size==(512,240), 'wrong framebuffer dimensions'
            image.save(path.with_suffix('.png'))
            hashes[name]=sha(path)
        assert len({hashes[name] for name in screen_names})==4, 'captured states did not change'
        marker_checks=re.findall(r'REORDER-MARKER-VERIFY (\S+) four glyph footprints checked; visible=(\d+)',result.stdout)
        assert marker_checks==[('entry.ppm','2'),('replacement-scrolled.ppm','3'),('swapped.ppm','3')], \
            'both original list marker pairs must survive focus changes and independent scroll'
        evidence['reorder_marker_checks']={'source_constructor':'0x8003DD38',
            'source_refresh':'0x8003A224','composed_original_font_footprints':12,
            'captures':marker_checks,'original_motion_parity':'not_verified'}
        footer_checks=[]
        for stage,parent,tag in [('first','entry','hel1'),('replacement','replacement-scrolled','hel2'),
                                 ('swapped','swapped','hel1')]:
            footer=Image.open(ROOT/'.local/assetpacks/menu/ZSET4-decoded'/f'{tag}.png').convert('RGBA')
            assert footer.size==(68,10), 'unexpected original numbered Help asset size'
            cases=[parent,f'help-{stage}-returned'] if stage!='swapped' else [parent]
            cases += [f'view-{stage}-returned',f'compare-{stage}-returned']
            checked=0
            for name in cases:
                rendered=Image.open(captures/f'{name}.ppm').convert('RGB')
                for y in range(footer.height):
                    for x in range(footer.width):
                        r,g,b,a=footer.getpixel((x,y))
                        if a and (r or g or b): # Same original black-transparent sprite format.
                            assert rendered.getpixel((235+x,217+y))==(r,g,b), \
                                f'{name}: Help footer must use source-selected {tag}, not generic help'
                            checked+=1
            assert checked>0, 'empty Help footer proof'
            footer_checks.append(dict(stage=stage,tag=tag,captures=cases,opaque_pixels_checked=checked))
        evidence['reorder_help_footer']={'table':'0x8009B230','selector':'0x8003D5F0',
            'graphic_replacement':'0x8003186C','object':4,'position':[235,217],
            'checks':footer_checks,'original_transition_timing':'not_verified'}
        help_evidence={}
        for stage, parent, bounds in [('first','entry',(121,70,391,210)),
                                     ('replacement','replacement-scrolled',(121,85,391,195))]:
            base_image=Image.open(captures/(parent+'.ppm')).convert('RGB')
            prefix='help-'+stage+'-'
            assert hashes[prefix+'returned']==hashes[parent], 'Help return changed frozen parent framebuffer'
            start_image=Image.open(captures/(prefix+'start.ppm')).convert('RGB')
            opened=Image.open(captures/(prefix+'open.ppm')).convert('RGB')
            closing=Image.open(captures/(prefix+'closing.ppm')).convert('RGB')
            assert ImageChops.difference(start_image,base_image).getbbox()==(246,110,266,120), 'wrong initial Help rectangle'
            assert ImageChops.difference(opened,base_image).getbbox()==bounds, 'Help content outside original modal bounds'
            assert opened.getpixel(bounds[:2])==(10,20,10), 'wrong Help gradient corner'
            assert hashes[prefix+'closing'] not in (hashes[prefix+'open'],hashes[prefix+'start']), 'no intermediate shrink frame'
            closing_bounds=ImageChops.difference(closing,base_image).getbbox()
            x,y,right,bottom=bounds
            expected=(min(246,x+45),min(110,y+20),
                      min(246,x+45)+max(20,right-x-90),min(110,y+20)+max(10,bottom-y-40))
            assert closing_bounds==expected, 'wrong five-tick Help shrink bounds'
            help_evidence[stage]={'full_bounds':bounds,'shrink_five_ticks_bounds':closing_bounds,
                'returned_frame_identical':True,'original_reference_comparison':'not_verified'}
        assert result.stdout.count('role=reorder-help-open FUN_8002F124 id=7')==8, 'wrong Help open sound dispatch'
        assert result.stdout.count('role=reorder-help-close FUN_8002F124 id=8')==8, 'wrong Help close sound dispatch'
        view_sounds=list(map(int,re.findall(r'role=view-input FUN_8002F124 id=(\d+)',result.stdout)))
        assert view_sounds==[1,6,6,6,6,6,2,1,6]+[1,6,6]*2, 'View input route sound mismatch'
        assert list(map(int,re.findall(r'role=view-return FUN_8002F124 id=(\d+)',result.stdout)))==[9,10,9,9,9], \
            'View Start/Cancel return sound mismatch'
        assert list(map(int,re.findall(r'PLAYER-STAT-FLASH.*?sound=(\d+)',result.stdout)))==[4,4,3,4,4,3], \
            'View endpoint/layer change generated extra scroll sound or flash'
        setting_lines=re.findall(r'ROSTER-CARD-SFX.*?role=sfx-setting-check[^\n]*',result.stdout)
        assert len(setting_lines)==2 and 'setting=8 playback=submitted' in setting_lines[0] and \
            'setting=0 playback=suppressed' in setting_lines[1], 'host SFX settings not wired'
        assert re.search(r'PLAYER-STAT-FLASH.*?sound=3[^\n]*setting=0 playback=suppressed',result.stdout), \
            'muted stat navigation lost its visual feedback'
        assert 'SFX-SETTING-VERIFY' in result.stdout, 'host SFX state assertions not executed'
        assert 'PLAYER-NOTICE-VERIFY' in result.stdout, 'notice parent/child barrier assertions not executed'
        assert hashes['no-facts-player']==hashes['no-facts-returned'], 'notice return changed player frame'
        assert hashes['no-facts-parent']==hashes['no-facts-editor-return'], 'notice child return changed editor frame'
        player_frame=Image.open(captures/'no-facts-player.ppm').convert('RGB')
        for suffix,bounds in [('opening',(246,110,266,120)),('open',(136,90,376,154)),('closing',(181,110,331,134))]:
            notice_frame=Image.open(captures/f'no-facts-{suffix}.ppm').convert('RGB')
            assert ImageChops.difference(player_frame,notice_frame).getbbox()==bounds, 'notice bounds/background changed'
            assert notice_frame.getpixel(bounds[:2])==(20,10,10), 'notice not original red style'
        assert list(map(int,re.findall(r'role=player-no-facts-(?:open|close) FUN_8002F124 id=(\d+)',result.stdout)))==[5,8], \
            'notice warning open/close sound mismatch'
        scans=re.findall(r'PLAYER-TEAM-SCAN.*?slot=(\d+)->(\d+) stat-top=(\d+)->(\d+)',result.stdout)
        assert len(scans)==4 and all(before==after for _,_,before,after in scans), 'View team scan lost stat top'
        assert not re.search(r'AUDIO-ERROR',result.stdout), 'native audio dispatch failed'
        view_evidence={}
        pushes=re.findall(r'push state=0x24 from=0x0C.*?page=(\d+) player=(\d+) slot=(\d+)',result.stdout)
        assert len(pushes)==5, 'missing actual View/notice/speech dispatch'
        cycles=re.findall(r'COOL-FACT-CYCLE-VERIFY cycle=(\d+) step=(\d+) variant=(\d+) played-mask=(\d+)',result.stdout)
        assert len(cycles)==10, 'missing actual speech cycles'
        for cycle in range(2):
            rows=[tuple(map(int,row[1:])) for row in cycles if int(row[0])==cycle]
            assert [r[0] for r in rows]==list(range(5)) and {r[1] for r in rows}==set(range(5)), 'repeated/skipped speech'
            mask=0
            for _,variant,observed in rows:
                mask|=1<<variant
                assert observed==mask, 'wrong consumed speech mask'
        assert cycles[4][2]!=cycles[5][2], 'speech refill immediately repeated last clip'
        assert hashes['fact-cycle-player']==hashes['fact-cycle-returned'], 'speech cycle changed View frame'
        assert hashes['fact-cycle-parent']==hashes['fact-cycle-editor-return'], 'speech cycle changed parent frame'
        assert result.stdout.count('role=cool-fact-select FUN_8002F124 id=6')==10, 'speech selection sound missing'
        speech_starts=[tuple(map(int,row)) for row in re.findall(
            r'COOL-FACT-START 31770 record=(\d+) speech-setting=(\d+) gain=(\d+)/127 playback=submitted',result.stdout)]
        assert len(speech_starts)==10, 'speech start route missing'
        assert [s[1] for s in speech_starts]==[9,8,4,0,9]*2 and \
            all(gain==min(setting*15,127) for _,setting,gain in speech_starts), 'speech setting ignored or wrong scale'
        speech_order=re.findall(r'(COOL-FACT-PREPARE|ROSTER-CARD-SFX role=cool-fact-select|COOL-FACT-START|COOL-FACT-FLASH 59EBC)',result.stdout)
        assert speech_order==['COOL-FACT-PREPARE','ROSTER-CARD-SFX role=cool-fact-select',
                              'COOL-FACT-START','COOL-FACT-FLASH 59EBC']*10, 'speech prepare/cue/start/flash ordering'
        stops=[tuple(map(int,row)) for row in re.findall(r'COOL-FACT-STOP .*?stopped=(\d+) cue=(\d+)',result.stdout)]
        assert stops==[(1,5),(0,0)]*10, 'Square feedback must distinguish active voice and idle'
        assert result.stdout.count('role=cool-fact-stop FUN_8002F124 id=5')==10, 'wrong Square stop cue dispatch'
        flash_events=[tuple(map(int,row)) for row in re.findall(
            r'COOL-FACT-FLASH completed-present=(\d+) overlay=(\d+) consumed=(\d+)',result.stdout)]
        assert flash_events==[(frame, int(frame<8 and frame%2==0), int(frame==8))
                              for _ in range(10) for frame in range(1,9)], 'flash/consumption ordering changed'
        resting=Image.open(captures/'fact-cycle-player.ppm').convert('RGBA')
        overlay_image=Image.open(ROOT/'.local/assetpacks/menu/ZSET8-decoded/o18b.png').convert('RGBA')
        # ZSET8 PNG export is opaque; the loader restores black-key transparency.
        overlay_image.putdata([(r,g,b,0 if (r,g,b)==(0,0,0) else a)
                               for r,g,b,a in overlay_image.getdata()])
        expected_flash=resting.copy()
        expected_flash.alpha_composite(overlay_image,(356,198))
        assert ImageChops.difference(expected_flash.convert('RGB'),resting.convert('RGB')).getbbox(), 'empty overlay fixture'
        for frame,name in enumerate(flash_names):
            rendered=Image.open(captures/(name+'.ppm')).convert('RGB')
            expected=(resting if frame%2 else expected_flash).convert('RGB')
            assert ImageChops.difference(rendered,expected).getbbox() is None, 'flash must overlay original o18b, retain o18a and change nothing else'
        notice_id=int(pushes[3][1])
        fact_index=(ROOT/'.local/assetpacks/menu/Z1COOL.IDX').read_bytes()
        fact_count,=struct.unpack_from('<I',fact_index)
        speech_exports=[]
        with (ROOT/'.local/assetpacks/menu/Z1COOL.BIG').open('rb') as speech_archive:
            for logical in range(6):
                player_id,variant=divmod(logical,5)
                physical=logical+1
                size,offset=struct.unpack_from('<II',fact_index,4+physical*8)
                speech_archive.seek(offset)
                speech_header=speech_archive.read(0x74)
                rate,=struct.unpack_from('<H',speech_header,0x42)
                samples,=struct.unpack_from('<I',speech_header,0x48)
                filename=f'cool-fact-p{player_id}-v{variant}.wav'
                with wave.open(str(captures/filename),'rb') as wav:
                    assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate(),wav.getnframes())==(1,2,rate,samples), \
                        'speech export differs from physical record header'
                assert f'player={player_id} variant={variant} logical={logical} physical={physical} rate={rate} samples={samples} -> {filename}' in result.stdout, \
                    'missing speech identity diagnostics'
                speech_exports.append(dict(player=player_id,variant=variant,logical=logical,physical=physical,
                    source_offset=offset,source_bytes=size,sample_rate=rate,samples=samples,file=filename,sha256=sha(captures/filename)))
        evidence['speech_index_exports']=speech_exports
        assert notice_id*5+4<fact_count, 'notice fixture not represented in original IDX'
        assert all(struct.unpack_from('<I',fact_index,4+(notice_id*5+v+1)*8)[0]==0 for v in range(5)), \
            'notice fixture has an original Cool Fact record'
        for route_index,(stage,parent,page) in enumerate([('first','entry',0),
                ('replacement','replacement-scrolled',1),('swapped','swapped',0)]):
            prefix='view-'+stage+'-'
            assert hashes[prefix+'returned']==hashes[parent], 'View return changed parent frame'
            assert hashes[prefix+'entered']!=hashes[parent], 'View child not rendered'
            assert hashes[prefix+'browsed']!=hashes[prefix+'entered'], 'child browsing did not change display'
            browsed=Image.open(captures/(prefix+'browsed.ppm')).convert('RGB')
            help_image=Image.open(captures/(prefix+'help.ppm')).convert('RGB')
            assert ImageChops.difference(browsed,help_image).getbbox()==(130,60,380,200), 'wrong child Help panel'
            parent_match=re.search(re.escape(parent)+r'\.ppm team=.*?cursor=(\d+)/(\d+).*?selected=(\d+)/(\d+)',result.stdout)
            assert parent_match, 'missing parent identity trace'
            parent_values=tuple(map(int,parent_match.groups()))
            child_page,child_id,child_slot=map(int,pushes[route_index])
            assert (child_page,child_id,child_slot)==(page,parent_values[2+page],parent_values[page]), 'child ignores parent active slot/draft'
            # Check opaque photograph pixels beyond the 39-pixel team-logo
            # overlay. This catches a correctly logged ID with the wrong art.
            portrait_path=ROOT/'.local/assetpacks/menu/Z1PORT-decoded'/f'player_{child_id+1:03}.png'
            portrait=Image.open(portrait_path).convert('RGBA')
            entered=Image.open(captures/(prefix+'entered.ppm')).convert('RGB')
            assert portrait.size==(180,156), 'wrong portrait archive'
            checked=0
            for y in range(156):
                for x in range(40,180):
                    rgba=portrait.getpixel((x,y))
                    if rgba[3]:
                        assert entered.getpixel((297+x,35+y))==rgba[:3], 'wrong child photograph pixels'
                        checked+=1
            assert checked>1000, 'insufficient portrait pixel evidence'
            # Source-layout evidence and actual composed pixels, not a timer
            # approximation or a replacement "please wait" graphic.
            for slot, expected in [(16,(35,296,0,7,b'atlZ',0)),
                                   (17,(35,297,1,3,b'shot',0)),
                                   (18,(35,334,1,13,b'wait',0))]:
                assert struct.unpack_from('<hhhh4sI',data,0x80097a24-base+slot*16)==expected, \
                    'View photo/city/wait source layout changed'
            wait_asset=ROOT/'.local/assetpacks/menu/ZSET8-decoded/wait.png'
            wait_sprite=Image.open(wait_asset).convert('RGBA')
            waiting=Image.open(captures/(prefix+'photo-wait.ppm')).convert('RGB')
            cycling=Image.open(captures/(prefix+'photo-cycle-wait.ppm')).convert('RGB')
            wait_pixels=0
            for y in range(wait_sprite.height):
                for x in range(wait_sprite.width):
                    rgba=wait_sprite.getpixel((x,y))
                    # The existing ZSET8 loader keys RGB zero transparent;
                    # decoded PNG alpha alone does not describe that mask.
                    if rgba[3] and rgba[:3]!=(0,0,0):
                        assert waiting.getpixel((334+x,35+y))==rgba[:3], 'missing/wrong loading asset'
                        if x>0: # city overlay's last column shares x334
                            assert cycling.getpixel((334+x,35+y))==rgba[:3], 'cycle did not hide old photo'
                        wait_pixels+=1
            assert wait_pixels>1000, 'insufficient loading-asset pixel evidence'
            city_bounds=(296,35,334,191)
            assert waiting.crop(city_bounds).tobytes()!=entered.crop(city_bounds).tobytes(), \
                'city must start hidden until first photo completes'
            assert re.search(re.escape(parent)+r'\.ppm team=3 ',result.stdout), 'city fixture is not Chicago'
            city_path=ROOT/'.local/assetpacks/menu/ZSET8-decoded/chiZ.png'
            city=Image.open(city_path).convert('RGBA')
            city_pixels=0
            for y in range(city.height):
                for x in range(city.width):
                    rgba=city.getpixel((x,y))
                    if rgba[3] and rgba[:3]!=(0,0,0):
                        assert cycling.getpixel((296+x,35+y))==entered.getpixel((296+x,35+y))==rgba[:3], \
                            'city must remain visible on subsequent player request'
                        city_pixels+=1
            assert city_pixels>1000, 'insufficient city-strip pixels'
            bounds=ImageChops.difference(waiting,entered).getbbox()
            assert bounds and bounds[0]>=296 and bounds[1]>=35 and bounds[2]<=477 and bounds[3]<=191, \
                'photo completion unexpectedly changed text/background/controls'
            view_evidence[stage]={'selected_player':child_id,'parent_page':page,'parent_slot':child_slot,
                'portrait_record':child_id+1,'portrait_sha256':sha(portrait_path),'opaque_pixels_checked':checked,
                'photo_loading':{'wait_asset_sha256':sha(wait_asset),'wait_pixels_checked':wait_pixels,
                    'city_asset_sha256':sha(city_path),'city_pixels_checked':city_pixels,
                    'initial_city_hidden':True,'cycle_city_retained':True,'completion_bounds':bounds,
                    'native_async_decode':True,'original_latency_parity':'not_verified'},
                'returned_frame_identical':True,'original_reference_comparison':'not_verified'}
        compare_evidence={}
        post_checks=re.findall(r'COMPARE-PACING-VERIFY (\S+) post-presents=(\d+);',result.stdout)
        assert post_checks==[('compare-first','8'),('compare-replacement','8'),('compare-swapped','8')], 'missing post-delay host checks'
        assert struct.unpack_from('<3H',data,0x80024f6c-base)==(16,14,18), 'original font spans changed'
        # 3D930 counts signed descriptor types>=64. A zero count enables
        # 3AE4C's selected display-object animation wait (2C668 ->2C610).
        # It is NOT a keyboard-release/held-input flag:2C610 only reads
        # flags+3B and animation pairs+26/+27 (unsigned),+29/+2A (signed).
        animation_wait_bypass_descriptors=0
        for i in range(57):
            pointer,=struct.unpack_from('<I',data,0x800a48b4-base+i*4)
            kind,=struct.unpack_from('<b',data,pointer-base+8)
            animation_wait_bypass_descriptors+=kind>=64
        assert animation_wait_bypass_descriptors==0, 'Compare animation-wait descriptor contract changed'
        assert result.stdout.count('selected-text wait=0 (2C244 mask=C7); held-repeat=enabled')>=3
        assert struct.unpack_from('<I',data,0x8002c37c-base)[0]==0x304200c7, '2C244 original flag-copy mask changed'
        refresh_frames=[Image.open(captures/(name+'.ppm')).convert('RGB') for name in refresh_names]
        assert hashes[refresh_names[0]]==hashes['compare-first-side'], 'first callback frame did not retain prior graphics/text'
        portrait_delta=ImageChops.difference(refresh_frames[0],refresh_frames[1]).getbbox()
        assert portrait_delta and portrait_delta[0]>=386 and portrait_delta[1]>=22 and portrait_delta[2]<=473 and portrait_delta[3]<=73, 'first completion changed more than active portrait'
        assert ImageChops.difference(refresh_frames[0].crop((0,75,512,240)),refresh_frames[1].crop((0,75,512,240))).getbbox() is None, 'text refreshed before two completed presentations'
        assert ImageChops.difference(refresh_frames[1].crop((256,75,512,210)),refresh_frames[2].crop((256,75,512,210))).getbbox(), 'text did not refresh after second presentation'
        assert refresh_frames[1].crop((386,22,473,73)).tobytes()==refresh_frames[2].crop((386,22,473,73)).tobytes(), 'text completion changed loaded portrait'
        assert len({f.crop((0,0,256,240)).tobytes() for f in refresh_frames})==1, 'player callback changed inactive half'
        requested=re.search(r'COMPARE-REFRESH 59928 requested player=(\d+) side=1',result.stdout)
        assert requested, 'missing callback identity trace'
        portrait=Image.open(ROOT/'.local/assetpacks/menu/Z2PORT-decoded'/f'player_{int(requested[1])+1:03}.png').convert('RGBA')
        refresh_photo_pixels=0
        for y in range(10,35):
            for x in range(15,60):
                pixel=portrait.getpixel((x,y))
                if pixel[3]:
                    assert refresh_frames[1].getpixel((386+x,22+y))==pixel[:3], 'pending callback loaded wrong local portrait'
                    refresh_photo_pixels+=1
        completions=re.findall(r'COMPARE-REFRESH completed-present=(\d+) text=(\d+)/(\d+) requested=(\d+)/(\d+) cue=(\d+)',result.stdout)
        assert len(completions)==6, 'three Compare callbacks must each complete exactly two presentations'
        for index,values in enumerate(completions):
            phase,tl,tr,rl,rr,cue=map(int,values)
            assert (phase,cue)==(index%2+1,index%2), 'text/sound callback phase mismatch'
            assert tl==rl and ((tr==rr) if phase==2 else (tr!=rr)), 'wrong retained/committed identity'
        markers=['compare-refresh-0.ppm team=', 'COMPARE-REFRESH completed-present=1',
            'compare-refresh-1.ppm team=', 'COMPARE-REFRESH completed-present=2',
            'role=compare-input FUN_8002F124 id=1', 'compare-refresh-2.ppm team=']
        position=-1
        for marker in markers:
            position=result.stdout.find(marker,position+1)
            assert position>=0, 'callback frame/text/sound event order differs'
        packed=(ROOT/'.local/assetpacks/menu/ZSET4-team-backgrounds/indexed.n97pal').read_bytes()
        assert len(packed)==134356 and struct.unpack_from('<4s6H',packed)==(b'N97P',1,4,33,128,240,0)
        banks=[struct.unpack_from('<160H',packed,20+t*324) for t in range(33)]
        def palette_pixel(strip,x,y,team,target=None,factor=None):
            start=16+33*324+strip*30912
            index=packed[start+y*128+x]
            if index>=160:
                word,=struct.unpack_from('<H',packed,start+30720+(index-160)*2)
            else:
                word=banks[team][index]
                if factor is not None:
                    dest=banks[target][index]
                    result=dest&0x8000
                    for mask in (0x001f,0x03e0,0x3c00):
                        product=((dest&mask)-(word&mask))*factor
                        # MIPS negative correction then arithmetic shift, not RGB averaging.
                        delta=(product+(15 if product<0 else 0))//16
                        result|=((word&mask)+delta)&mask
                    word=result
            return tuple(((word>>s)&31)*8+(((word>>s)&31)>>2) for s in (0,5,10))
        palette_checked=0
        compare_start,=struct.unpack_from('<I',data,0x80093330-base+35*4)
        y,x,enabled,depth=struct.unpack_from('<hhhh',data,compare_start-base+5*16)
        assert (x,y,enabled,depth)==(170,15,1,3) and data[compare_start-base+88:compare_start-base+92]==b'ba02', 'wrong Compare graphics table'
        for stage,parent in [('first','entry'),('replacement','replacement-scrolled'),('swapped','swapped')]:
            prefix='compare-'+stage+'-'
            assert hashes[prefix+'returned']==hashes[prefix+'parent'], 'Compare return changed immediate pre-entry parent frame'
            entered=Image.open(captures/(prefix+'entered.ppm')).convert('RGB')
            side=Image.open(captures/(prefix+'side.ppm')).convert('RGB')
            browsed=Image.open(captures/(prefix+'browsed.ppm')).convert('RGB')
            panel=Image.open(captures/(prefix+'help.ppm')).convert('RGB')
            assert ImageChops.difference(browsed,panel).getbbox()==(130,75,380,205), 'wrong Compare Help modal bounds'
            diff=ImageChops.difference(entered,side)
            assert diff.getbbox()==(113,116,398,127), 'Cross selector bounds differ from original glyph metrics'
            # Only the recovered left/right selector pair moves when Cross is pressed.
            for box in [(113,116,142,127),(369,116,398,127)]:
                assert diff.crop(box).getbbox(), 'missing one active-side arrow pair'
                diff.paste((0,0,0),box)
            assert diff.getbbox() is None, 'Cross altered data/art outside selector pairs'
            parent_match=re.search(re.escape(parent)+r'\.ppm team=(\d+).*?selected=(\d+)/(\d+)',result.stdout)
            assert parent_match, 'missing Compare parent IDs'
            team,*ids=map(int,parent_match.groups())
            photos=[]
            for p,player in enumerate(ids):
                path=ROOT/'.local/assetpacks/menu/Z2PORT-decoded'/f'player_{player+1:03}.png'
                photo=Image.open(path).convert('RGBA')
                assert photo.size==(87,51), 'Compare used large View portrait'
                checked=0
                for py in range(10,35):
                    for px in range(15,60):
                        rgba=photo.getpixel((px,py))
                        if rgba[3]:
                            assert entered.getpixel(((386 if p else 54)+px,22+py))==rgba[:3], 'Compare portrait mismatches draft'
                            checked+=1
                assert checked>900, 'insufficient Compare portrait coverage'
                photos.append({'player':player,'record':player+1,'sha256':sha(path),'pixels_checked':checked})
            match=re.search(re.escape(prefix+'browsed.ppm')+r' active=(\d+) teams=(\d+)/(\d+) slots=(\d+)/(\d+) players=(\d+)/(\d+) layer=(\d+) top=(\d+)',result.stdout)
            assert match, 'missing Compare navigation trace'
            active,left,right,ls,rs,lp,rp,layer,top=map(int,match.groups())
            assert (active,left,right,lp,layer,top)==(1,team,team+1,ids[0],3,1), 'Compare navigation changed wrong side/layer/scroll'
            if stage=='first':
                frozen_left=None
                for factor in range(17):
                    frame=Image.open(captures/f'compare-palette-{factor}.ppm').convert('RGB')
                    if factor in (0,8,16):
                        frame.save(captures/f'compare-palette-{factor}.png') # Lossless inspection copies.
                    for strip,x0 in ((0,42),(3,465)):
                        for py in range(128,180):
                            for px in range(x0,x0+8):
                                expected=palette_pixel(strip,px-strip*128,py,team,
                                    right if strip==3 else None,factor if strip==3 else None)
                                assert frame.getpixel((px,py))==expected, f'Compare palette factor {factor} differs from raw CLUT math'
                                palette_checked+=1
                    left_pixels=frame.crop((0,0,256,240)).tobytes()
                    if frozen_left is None: frozen_left=left_pixels
                    assert left_pixels==frozen_left, 'right fade altered inactive half'
                assert hashes[palette_names[0]]!=hashes[palette_names[-1]], 'palette sequence never animated'
                assert hashes[palette_names[-1]]==hashes['compare-palette-settled'], 'settled render differs from factor16'
            codes=('atl','bos','cha','chi','cle','dal','den','det','gol','hou','ind','lac','lal','mia','mil','min','nwj','nwy','orl','phi','pho','por','sac','san','sea','tor','uta','van','was','xea')
            for team_id,box,strip,offset in [(left,(42,128,50,180),'Bkga',0),(right,(465,128,473,180),'Bkgd',384)]:
                source=Image.open(ROOT/'.local/assetpacks/menu/ZSET4-team-backgrounds'/codes[team_id]/(strip+'.png')).convert('RGB')
                wanted=source.crop((box[0]-offset,box[1],box[2]-offset,box[3]))
                assert ImageChops.difference(browsed.crop(box),wanted).getbbox() is None, 'Compare palette halves do not follow separate teams'
            compare_evidence[stage]={'photos':photos,'parent_return_identical':True,'active_side_arrows_only':True,
                'independent_palette_halves':True,'help_bounds':[130,75,380,205],'original_reference_comparison':'not_verified'}
        compare_sounds=list(map(int,re.findall(r'role=compare-input FUN_8002F124 id=(\d+)',result.stdout)))
        assert compare_sounds==[6,1,6,6,4]+[6]*3+[4]*9+[6]*5+[6,1,6,6,4]*2, 'Compare sounds/endpoint suppression differ from source mapping'
        for name,layer,top in [('compare-ratings',1,0),('compare-attributes',0,0),('compare-attributes-bottom',0,9),('compare-free-agents',0,9)]:
            assert re.search(re.escape(name)+r'\.ppm active=1 .*?layer='+str(layer)+r' top='+str(top)+r';',result.stdout), 'wrong extra Compare layer/scroll'
        assert len({hashes[name] for name in compare_extra})==4, 'Compare extra states not rendered'
        free=Image.open(captures/'compare-free-agents.ppm').convert('RGB')
        palette=Image.open(ROOT/'.local/assetpacks/menu/ZSET4-team-backgrounds/xea/Bkgd.png').convert('RGB')
        assert ImageChops.difference(free.crop((465,128,473,180)),palette.crop((81,128,89,180))).getbbox() is None, 'free-agent palette not original record29'
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
                'menu/ZCURSOR.VH','menu/ZCURSOR.VB',
                'player/no-facts.n97ui','menu/Z1COOL.IDX','menu/Z1COOL.BIG',
                'menu/ZSET8-decoded/o18a.png','menu/ZSET8-decoded/o18b.png',
                'menu/ZSET8-decoded/wait.png','menu/ZSET8-decoded/ba41.png',
                'menu/ZSET4-decoded/ba22.png','menu/ZSET4-decoded/ba02.png','menu/ZSET4-decoded/ba35.png',
                'menu/ZSET4-decoded/hel1.png','menu/ZSET4-decoded/hel2.png',
                'menu/ZSET4.PSP','menu/ZSET8.PSP','menu/ZTMPAL.PSH','menu/Z2PORT.IDX','menu/Z2PORT.BIG',
                'menu/ZSET4-team-backgrounds/indexed.n97pal',
                'menu/Z1PORT.IDX','menu/Z1PORT.BIG',
                'reorder/dialogs.n97ui','reorder/discard.n97ui','reorder/help.n97ui','reorder/compare.n97ui']
        evidence.update(status='passed',graphics_state=12,input_layout=13,
            original_layout_address=hex(start),layout_records=len(records),
            capture_sha256=hashes,shared_helper_counters=state_counts,help_round_trips=help_evidence,
            help_sound_dispatch={'open':7,'close':8,'original_recorded_audio_comparison':'not_verified'},
            help_native_checks=help_checks,help_test_executable_sha256=sha(help_exe),
              view_round_trips=view_evidence,child_native_checks=child_checks,
              view_input_sounds=view_sounds,view_team_scan_states=scans,
            child_test_executable_sha256=sha(child_exe),
            audio_test_executable_sha256=sha(audio_exe),audio_host_setting_checks=setting_lines,
            player_notice_checks=notice_checks,player_notice_test_sha256=sha(notice_exe),
            cool_fact_selection_checks=fact_checks,cool_fact_selection_test_sha256=sha(fact_exe),
            cool_fact_cycles=cycles,cool_fact_rng_parity='not_verified_native_stream',
            cool_fact_starts=speech_starts,cool_fact_stop_feedback=stops,
            cool_fact_host_order=speech_order,
            cool_fact_flash={'events':flash_events,'source_layout':hex(player_layout),'object_index':21,
                'base_retained':True,'captures':flash_names,'original_cadence_parity':'not_verified'},
            player_notice_case={'player_id':notice_id,'empty_original_records':5,'descriptor':'0x800AFE06',
                'continuation_prompt':'0x8002502C','body_y':[105,117],'prompt_y':135},
            player_notice_reference_parity='not_verified',
            compare_controller_checks=compare_checks,compare_ui_status='native_round_trips_verified',
            compare_scroll={'synthetic_sequences':236,'synthetic_endpoints':16,'host_actions':scroll_actions,
                'host_presents':251,'host_cues':scroll_sounds,'capture_sha256':scroll_hashes,
                'synthetic_null_up_polls':320,'first_row_up_callback_ids':[0,33],
                'font_sha256':sha(scroll_font),'font_records_checked':len(heights),'max_glyph_height':max(heights),
                'vertical_marker_pixel_checks':scroll_arrow_pixels,
                'source_block_checks':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in scroll_source_blocks],
                'group_order_pixel_checks':'passed','original_live_scroll_comparison':'not_verified',
                'top_endpoint_policy':'5A1EC clears both first-row Up callbacks; 3D930 bypasses callback/cue/delay; poll1 remains'},
            compare_generic_pacing={'delay_presents':5,'poll_presents':1,'synthetic_callback_masks':65024,
                'host_waits':generic_waits,'host_total_presents':96,'host_sounds':generic_sounds,
                'source_block_checks':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in callback_source_blocks],
                'source_callback_descriptors_checked':48,'native_counter_history_shared':True,
                'original_live_callback_comparison':'not_verified'},
            compare_refresh={'synthetic_sequences':116,'completions':completions,
                'captured_phases':refresh_names,'portrait_pixels_checked':refresh_photo_pixels,
                'retained_text_two_presents':True,'original_cd_delivery_timing':'not_verified'},
            compare_left_right_pacing={'synthetic_actions':40,'post_and_poll_presents':120,
                'normal_counter_record_passes':2,'fresh_counter':2,'repeat_counter_step':4,
                'counter_state_vectors':150,'rejected_masks':65534,
                'source_block_checks':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h}
                    for a,b,h in repeat_source_blocks],
                'host_post_checks':post_checks,
                'animation_wait_bypass_descriptors':animation_wait_bypass_descriptors,
                'animation_predicate_vectors':16777216,'rebuilt_flag_vectors':256,
                'animation_host_mapping':'player_cycle_rebuild_clears_wait_bits_2C244',
                'host_held_repeat':'enabled_left_right',
                'host_held_actions':40,'host_held_presentations':200,'host_extra_callbacks':45,
                'host_held_sounds':held_sounds,'host_held_checks':['wrap','release','reversal','chord','help','cancel'],
                'delays':[7,5,3,1],'counter_thresholds':[16,28,38],
                'original_live_held_input_comparison':'not_verified'},
            compare_palette={'factors':list(range(17)),'raw_clut_pixels_checked':palette_checked,
                'inactive_half_unchanged':True,'original_motion_parity':'not_verified'},
            compare_arrow_flash={'captures':arrow_hashes,'glyph_pixels_checked':int(arrow_proof[1]),
                'original_glyph_bounds':arrow_bounds,
                'arbitrary_initial_colors_tested':256,'updates_to_settle':21,'retrigger_checked':True,
                'outside_arrow_unchanged':True,'source':'2ADEC/2AC2C/2AE5C',
                'native_initial_modulation':[128,128,128],'original_allocator_initial_color':'not_verified',
                'original_live_motion_comparison':'not_verified','tint_test_executable_sha256':sha(tint_exe)},
            compare_round_trips=compare_evidence,compare_sound_dispatch=compare_sounds,
            compare_extra_captures=compare_extra,compare_asset_checks=pack_checks,compare_asset_test_sha256=sha(pack_exe),
            compare_test_executable_sha256=sha(compare_exe),
            database_unchanged=True,executable_sha256=sha(exe),
            original_overlay_sha256=sha(overlay),
            asset_sha256={name:sha(ROOT/'.local/assetpacks'/name) for name in assets},
            source_sha256={name:sha(ROOT/name) for name in ['src/win32_main.cpp','src/main_menu.cpp',
                'src/frontend_plate.cpp','src/frontend_plate.hpp','tests/frontend_plate_test.cpp',
                'src/recovered_audio.cpp','src/recovered_audio.hpp','src/psx_adpcm.cpp',
                'src/recovered/frontend_audio.c','src/recovered/frontend_audio.h',
                'src/frontend_settings.cpp','src/frontend_settings.hpp','tests/recovered_audio_test.cpp',
                'src/player_notice.cpp','src/player_notice.hpp','tests/player_notice_test.cpp',
                'src/cool_fact_index.hpp',
                'src/frontend_title.hpp','src/recovered/frontend_title.c','src/recovered/frontend_title.h',
                'tests/frontend_title_test.cpp',
                'src/player_photo_loader.hpp','src/recovered/player_photo.c','src/recovered/player_photo.h',
                'tests/player_photo_test.cpp',
                'src/recovered/cool_fact_selection.c','src/recovered/cool_fact_selection.h','tests/cool_fact_selection_test.cpp',
                'tools/extract_player_notice.py','tools/test_player_notice.py',
                'src/reorder_preview.cpp','src/recovered/reorder_screen.c','src/recovered/roster_reorder.c',
                'src/recovered/roster_lists.c','src/recovered/roster_lists.h',
                'src/frontend_help.cpp','src/frontend_help.hpp','src/recovered/frontend_help.c',
                'src/recovered/frontend_help.h','tools/extract_frontend_help.py',
                'tests/frontend_help_test.cpp','tools/test_frontend_help.py',
                'tests/reorder_child_test.cpp','src/recovered/reorder_children.c',
                'tests/roster_compare_test.cpp','src/recovered/roster_compare.c','src/recovered/roster_compare.h',
                'src/compare_assets.cpp','src/compare_assets.hpp','tools/extract_compare_assets.py',
                'src/frontend_palette_assets.cpp','src/frontend_palette_assets.hpp',
                'src/recovered/frontend_palette.c','src/recovered/frontend_palette.h',
                'tests/frontend_palette_test.cpp','tests/frontend_palette_assets_test.cpp',
                'tools/decode_team_backgrounds.py','tools/test_team_backgrounds.py',
                'tests/compare_assets_test.cpp','tools/test_compare_assets.py',
                'src/recovered/reorder_children.h','src/roster_database.cpp','src/roster_database.hpp','src/main_menu.hpp',
                'tools/decode_reorder_portraits.py','tools/verify_reorder_screen.py']})
    except Exception as error:
        evidence.update(status='failed',error=str(error))
        raise
    finally:
        output.write_text(json.dumps(evidence,indent=2)+'\n')
    print('REORDER SCREEN PASS: 129 frames (120 earlier checkpoints plus9 title frames); title vertices/holds, player/generic/scroll waits, held-input runs and17 palette factors; parent returns, original glyphs, selectors, modal bounds and sound dispatch; sources/assets hashed.')
    print('SFX gain/pitch pipeline checked; no original waveform/visual parity or full-feature completion inferred.')


if __name__=='__main__':
    main()
