"""Validate retained Windows process-mix packets, not original audio fidelity."""
import argparse
from array import array
import csv
import json
import math
from pathlib import Path
import sys
import wave

from verify_reorder_reference import read_json, private_path, require, sha

ROOT = Path(__file__).resolve().parents[1]


def inspect(directory, video_origin=None, video_times=None, test_tones=False, selected_pid=None, selected_tone=False):
    directory = directory.resolve(strict=True)
    directory.relative_to((ROOT / '.local').resolve(strict=True))
    report = read_json(private_path(directory, 'recording.json'))
    require(isinstance(report, dict), 'audio summary must be an object')
    require(report.get('schema_version') == 1 and report.get('kind') == 'windows_process_mix', 'wrong audio schema')
    require(report.get('complete') is True, 'audio capture is incomplete')
    expected_scope = 'include_selected_process_tree' if selected_pid is not None else 'include_current_process_tree'
    require(report.get('scope') == expected_scope and report.get('microphone') is False
            and report.get('system_fallback') is False, 'unexpected audio capture scope')
    if selected_pid is not None:
        require(type(selected_pid) is int and selected_pid > 0 and report.get('target_pid') == selected_pid,
                'selected audio process does not match explicit expected PID')
        require(report.get('target_executable_verified') is True and
                type(report.get('target_creation_filetime')) is int and report['target_creation_filetime'] > 0,
                'selected audio process identity was not verified by recorder')
    require(report.get('windows_autoconvert_pcm') is True and report.get('original_parity') is False
            and report.get('sample_continuity_verified') is False, 'unsupported audio fidelity claim')
    require((report.get('sample_rate'), report.get('channels'), report.get('bits')) == (48000, 2, 16), 'wrong PCM format')
    for key in ('qpc_origin_100ns', 'target_pid', 'packets', 'sample_frames', 'requested_end_qpc_100ns'):
        require(type(report.get(key)) is int and report[key] > 0, f'invalid {key}')
    require(report['packets'] <= 20000 and report['sample_frames'] <= 48000 * 120, 'audio exceeds recording bound')
    origin = report['qpc_origin_100ns']
    if video_origin is not None:
        require(origin == video_origin, 'video/audio clocks have different origins')
    timeline = private_path(directory, 'packets.csv')
    require(timeline.stat().st_size <= 4 * 1024 * 1024, 'oversized audio timeline')
    with timeline.open(newline='', encoding='ascii') as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) == report['packets'], 'audio packet count mismatch')
    packets, offset, expected_position, gaps, nonzero_positions = [], 0, 0, 0, 0
    discontinuities, timestamp_errors, qpc_residuals = 0, 0, []
    prior_qpc, prior_count = None, 0
    for index, row in enumerate(rows):
        require(set(row) == {'packet', 'sample_offset', 'frames', 'device_position', 'qpc_100ns', 'flags'}, 'wrong packet columns')
        r = {key: int(value) for key, value in row.items()}
        require(r['packet'] == index and r['sample_offset'] == offset, 'packet samples are missing/reordered')
        count, position, qpc, flags = r['frames'], r['device_position'], r['qpc_100ns'], r['flags']
        require(0 < count <= 48000 and position >= 0 and qpc > 0 and 0 <= flags <= 7, 'invalid packet values')
        if prior_qpc is not None:
            require(qpc > prior_qpc, 'audio QPC did not advance')
            qpc_residuals.append((qpc - prior_qpc) / 10 - prior_count * 1000000 / 48000)
            gaps += int(position != expected_position)
        nonzero_positions += int(position != 0)
        discontinuities += int(bool(flags & 1))
        timestamp_errors += int(bool(flags & 4))
        offset += count
        expected_position, prior_qpc, prior_count = position + count, qpc, count
        packets.append(r)
    require(offset == report['sample_frames'], 'packet sample count mismatch')
    require((gaps, discontinuities, timestamp_errors) == (report['position_gaps'], report['discontinuities'], report['timestamp_errors']), 'audio status counters disagree with packets')
    status = 'reported' if nonzero_positions else 'unavailable_all_zero'
    require(status == report['device_position_status'], 'device-position availability misreported')
    require(not discontinuities and not timestamp_errors and (not nonzero_positions or not gaps), 'audio glitches require review')
    last_end = packets[-1]['qpc_100ns'] + packets[-1]['frames'] * 10000000 // 48000
    require(report['first_packet_qpc_100ns'] == packets[0]['qpc_100ns'] and
            report['last_packet_qpc_100ns'] == packets[-1]['qpc_100ns'] and
            report['last_packet_end_qpc_100ns'] == last_end, 'packet endpoint summary mismatch')
    require(origin <= report['requested_end_qpc_100ns'] <= last_end, 'audio tail is incomplete')
    path = private_path(directory, 'mixed.wav')
    require(path.stat().st_size == 44 + offset * 4, 'truncated/extended audio payload')
    with wave.open(str(path), 'rb') as stream:
        require((stream.getnchannels(), stream.getsampwidth(), stream.getframerate(), stream.getnframes(), stream.getcomptype())
                == (2, 2, 48000, offset, 'NONE'), 'WAV format/count mismatch')
        pcm = array('h', stream.readframes(offset))
    if sys.byteorder != 'little':
        pcm.byteswap()
    require(len(pcm) == offset * 2, 'short PCM data')
    nonzero = sum(sample != 0 for sample in pcm)
    require(nonzero == report['nonzero_samples'], 'PCM signal count mismatch')
    for packet in packets:
        if packet['flags'] & 2:
            begin = packet['sample_offset'] * 2
            require(not any(pcm[begin:begin + packet['frames'] * 2]), 'silent packet contains nonzero samples')
    result = dict(status='process_mix_artifacts_valid', sample_frames=offset, packets=len(packets),
                  capture_scope=expected_scope, target_pid=report['target_pid'],
                  nonzero_samples=nonzero, wav_sha256=sha(path), device_position_status=status,
                  peak_abs_sample=max(map(abs, pcm)),
                  rms_sample=math.sqrt(sum(sample * sample for sample in pcm) / len(pcm)),
                  samples_above_one_lsb=sum(abs(sample) > 1 for sample in pcm),
                  sample_continuity_verified=False, original_parity=False,
                  windows_autoconvert_pcm=True,
                  qpc_interval_residual_us=dict(min=min(qpc_residuals), max=max(qpc_residuals)) if qpc_residuals else None)
    if video_times is not None:
        require(video_origin is not None and video_times, 'missing video clock')
        # Packet timestamp intervals, never a latency-search or normalized WAV.
        # Preserve ambiguous overlap or uncovered boundaries rather than invent
        # a single audio sample offset for the whole variable-rate video.
        missing, ambiguous = [], []
        for index, ns in enumerate(video_times):
            require(type(ns) is int and ns >= 0, 'invalid video timestamp')
            absolute_ns = origin * 100 + ns
            matches = sum(0 <= absolute_ns - p['qpc_100ns'] * 100 and
                          (absolute_ns - p['qpc_100ns'] * 100) * 48000 < p['frames'] * 1000000000
                          for p in packets)
            if matches == 0: missing.append(index)
            if matches > 1: ambiguous.append(index)
        result['video_boundary_coverage'] = dict(uncovered_frames=missing, overlapping_packet_frames=ambiguous)
        result['av_timing_verified'] = False
    require(not (test_tones and selected_tone), 'choose only one tone expectation')
    if test_tones or selected_tone:
        require(offset >= 96000, 'tone test is too short')
        levels = {}
        for channel in range(2):
            levels[channel] = {}
            for frequency in (440, 660, 880):
                signal = sum(complex(math.cos(2 * math.pi * frequency * i / 48000),
                                     math.sin(2 * math.pi * frequency * i / 48000)) * pcm[2 * (48000 + i) + channel] for i in range(48000))
                levels[channel][frequency] = 2 * abs(signal) / 48000
        if selected_tone:
            require(selected_pid is not None, 'selected tone requires an explicit target PID')
            require(levels[0][660] > 500, 'selected child tone missing')
            require(all(levels[c][f] < 1 for c in (0,1) for f in (440,880)), 'recorder-process tones leaked into selected capture')
            require(levels[1][660] < 1, 'selected mono-left tone leaked to right channel')
        else:
            require(levels[0][440] > 500 and levels[1][880] > 500, 'expected two in-process streams missing')
            require(levels[0][660] < 1 and levels[1][660] < 1, 'foreign-process tone present')
        result['test_tone_amplitudes'] = levels
        result['tone_test'] = 'selected_child_isolation_passed' if selected_tone else 'passed; requires separate evidence that the foreign process played'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--expect-test-tones', action='store_true')
    parser.add_argument('--selected-pid', type=int, help='explicit selected-process capture; never inferred from its sidecar')
    parser.add_argument('--expect-selected-tone', action='store_true')
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.directory, test_tones=args.expect_test_tones, selected_pid=args.selected_pid,
                                 selected_tone=args.expect_selected_tone), indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError, wave.Error) as error:
        print(f'PROCESS AUDIO INCOMPLETE: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
