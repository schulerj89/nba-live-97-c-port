#!/usr/bin/env python3
"""Build a private, versioned roster pack from NBA Live 97's FEONLY overlay.

The output contains copyrighted names and game data and must remain under
`.local/`.  The public C++ loader deliberately contains no fallback roster.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


PLAYER_OFFSET = 0x9D820
PLAYER_COUNT = 493       # FEONLY FUN_8005FE14 boundary: 0x1ed
TEAM_OFFSET = 0xEAB0     # runtime 0x80023ab0 at FEONLY base 0x80015000
TEAM_COUNT = 29
TEAM_STRIDE = 0x68
OVERLAY_BASE = 0x80015000

MAGIC = b"N97RDB\0\0"
ENDIAN_MARKER = 0x12345678
PLAYER_RECORD = struct.Struct("<HHH6B H 17B 10B 5I")
TEAM_RECORD = struct.Struct("<HH5I15h20B")


def cstring(data: bytes, offset: int) -> tuple[str, int]:
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"unterminated string at FEONLY offset 0x{offset:x}")
    return data[offset:end].decode("ascii"), end + 1


class StringPool:
    def __init__(self) -> None:
        self.data = bytearray(b"\0")
        self.offsets = {"": 0}

    def add(self, value: str) -> int:
        if value in self.offsets:
            return self.offsets[value]
        offset = len(self.data)
        self.data.extend(value.encode("utf-8") + b"\0")
        self.offsets[value] = offset
        return offset


def pointer_string(data: bytes, pointer: int) -> str:
    if pointer == 0:
        return ""
    offset = pointer - OVERLAY_BASE
    if offset < 0 or offset >= len(data):
        raise ValueError(f"team string pointer 0x{pointer:08x} is outside FEONLY")
    return cstring(data, offset)[0]


def build_pack(data: bytes) -> bytes:
    if len(data) != 959960:
        raise ValueError(f"expected 959960-byte US FEONLY.BIN, got {len(data)}")

    strings = StringPool()
    players = bytearray()
    at = PLAYER_OFFSET
    for expected_id in range(PLAYER_COUNT):
        start = at
        prefix = data[at:at + 41]
        if len(prefix) != 41:
            raise ValueError("truncated player prefix")
        player_id, art_index, portrait_index = struct.unpack_from("<HHH", prefix)
        if player_id != expected_id:
            raise ValueError(
                f"player sequence broke at {expected_id}: found {player_id} at 0x{at:x}")
        at += 41
        text = []
        for _ in range(5):
            value, at = cstring(data, at)
            text.append(strings.add(value))
        if at & 1:
            at += 1

        players.extend(PLAYER_RECORD.pack(
            player_id, art_index, portrait_index,
            *prefix[6:12], struct.unpack_from("<H", prefix, 12)[0],
            *prefix[14:31], *prefix[31:41], *text))
        if at <= start:
            raise ValueError("player parser made no progress")

    teams = bytearray()
    next_player = 0
    for team_id in range(TEAM_COUNT):
        offset = TEAM_OFFSET + team_id * TEAM_STRIDE
        roster_count = struct.unpack_from("<H", data, offset + 60)[0]
        if roster_count > 15:
            raise ValueError(f"team {team_id} has invalid roster count {roster_count}")
        names = [strings.add(pointer_string(data, struct.unpack_from(
            "<I", data, offset + field)[0])) for field in (64, 68, 72, 76, 80)]
        roster = list(range(next_player, next_player + roster_count))
        roster.extend([-1] * (15 - roster_count))
        next_player += roster_count
        teams.extend(TEAM_RECORD.pack(
            team_id, roster_count, *names, *roster, *data[offset + 84:offset + 104]))

    if next_player != 348:
        raise ValueError(f"expected 348 assigned players, recovered {next_player}")

    sections = [(b"PLAY", bytes(players), PLAYER_COUNT, PLAYER_RECORD.size),
                (b"TEAM", bytes(teams), TEAM_COUNT, TEAM_RECORD.size),
                (b"STRS", bytes(strings.data), len(strings.offsets), 0)]
    header_size = 24 + len(sections) * 20
    directory = bytearray()
    payload = bytearray()
    offset = header_size
    for tag, body, count, stride in sections:
        directory.extend(struct.pack("<4sIIII", tag, offset, len(body), count, stride))
        payload.extend(body)
        offset += len(body)
    return (struct.pack("<8sIIII", MAGIC, 1, ENDIAN_MARKER, len(sections), offset) +
            directory + payload)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("feonly", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pack = build_pack(args.feonly.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(pack)
    print(f"roster pack: 29 teams, 493 players, 348 assigned, 145 free agents -> {args.output}")


if __name__ == "__main__":
    main()
