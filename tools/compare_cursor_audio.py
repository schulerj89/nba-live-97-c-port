#!/usr/bin/env python3
"""Compare raw and authored-pitch ZCURSOR captures without publishing assets."""

from __future__ import annotations

import argparse
import json
import wave
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[1]


def read_wav(path: Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as wav:
        rate = wav.getframerate()
        pcm = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").astype(float)
    return rate, pcm / 32768.0


def metrics(rate: int, pcm: np.ndarray) -> dict[str, float]:
    if not len(pcm):
        return {"duration_ms": 0.0, "rms": 0.0, "peak": 0.0,
                "spectral_centroid_hz": 0.0}
    windowed = pcm * np.hanning(len(pcm))
    spectrum = np.abs(np.fft.rfft(windowed))
    frequencies = np.fft.rfftfreq(len(pcm), 1.0 / rate)
    centroid = float(np.sum(frequencies * spectrum) / max(np.sum(spectrum), 1e-12))
    return {
        "duration_ms": round(len(pcm) * 1000.0 / rate, 3),
        "rms": round(float(np.sqrt(np.mean(pcm * pcm))), 6),
        "peak": round(float(np.max(np.abs(pcm))), 6),
        "spectral_centroid_hz": round(centroid, 2),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", default=
                        ".local/verification/rosters_menu/native")
    parser.add_argument("--output-dir", default=
                        ".local/verification/rosters_menu/audio_analysis")
    args = parser.parse_args()
    capture = ROOT / args.capture_dir
    output = ROOT / args.output_dir
    output.mkdir(parents=True, exist_ok=True)
    metadata = json.loads((capture / "capture.json").read_text(encoding="utf-8"))
    report = {"method": "raw ADPCM decode vs recovered PATl/Tone pitch", "sounds": []}

    figure, axes = plt.subplots(6, 2, figsize=(12, 14), constrained_layout=True)
    for index, row in enumerate(metadata["sounds"]):
        rate, authored = read_wav(capture / row["file"])
        raw_rate, raw = read_wav(capture / row["raw_file"])
        raw_metrics = metrics(raw_rate, raw)
        authored_metrics = metrics(rate, authored)
        report["sounds"].append({
            "id": row["id"], "role": row["role"],
            "root_note": row["root_note"], "requested_note": row["requested_note"],
            "pitch_cents": row["pitch_cents"], "raw": raw_metrics,
            "authored": authored_metrics,
            "duration_ratio": round(authored_metrics["duration_ms"] /
                                    raw_metrics["duration_ms"], 5),
        })
        raw_time = np.arange(len(raw)) / raw_rate * 1000.0
        authored_time = np.arange(len(authored)) / rate * 1000.0
        axes[index, 0].plot(raw_time, raw, linewidth=0.55, color="#777777")
        axes[index, 0].plot(authored_time, authored, linewidth=0.55,
                            color="#d6b600", alpha=0.8)
        axes[index, 0].set_title(f"ID {row['id']} {row['role']}: raw vs authored")
        axes[index, 0].set_xlabel("milliseconds")
        axes[index, 0].set_ylim(-1, 1)
        for samples, color, label in ((raw, "#777777", "raw"),
                                      (authored, "#d6b600", "authored")):
            spectrum = np.abs(np.fft.rfft(samples * np.hanning(len(samples))))
            frequency = np.fft.rfftfreq(len(samples), 1.0 / rate)
            axes[index, 1].plot(frequency, 20 * np.log10(spectrum + 1e-8),
                                linewidth=0.65, color=color, label=label)
        axes[index, 1].set_xlim(0, 8000)
        axes[index, 1].set_ylim(-40, 70)
        axes[index, 1].set_title(f"pitch={row['pitch_cents']} cents")
        axes[index, 1].set_xlabel("Hz")
        axes[index, 1].legend(loc="upper right")

    report_path = output / "cursor_audio_comparison.json"
    plot_path = output / "cursor_audio_waveforms.png"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    figure.savefig(plot_path, dpi=150)
    print(f"Wrote {report_path}")
    print(f"Wrote {plot_path}")
    for row in report["sounds"]:
        print(f"  id={row['id']} {row['role']:<12} pitch={row['pitch_cents']:>4}c "
              f"duration={row['raw']['duration_ms']:.2f}->{row['authored']['duration_ms']:.2f}ms "
              f"centroid={row['raw']['spectral_centroid_hz']:.1f}->"
              f"{row['authored']['spectral_centroid_hz']:.1f}Hz")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
