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
TEAM_ROSTER_OFFSET = 0xABCAC  # DAT_800C0CAC, copied by FEONLY FUN_80057864
TEAM_ROSTER_SLOTS = 15
OVERLAY_BASE = 0x80015000
SCHOOL_POINTER_TABLE = 0x8009D87C - OVERLAY_BASE
# The four pointers at 0x8009D848 target the acquisition strings beginning
# around 0x8009D858/0x8009D860. Player byte +37 stores codes 250..253.
ACQUISITION_POINTER_TABLE = 0x8009D848 - OVERLAY_BASE
ACQUISITION_CODE_BASE = 250
ACQUISITION_METHOD_COUNT = 4

MAGIC = b"N97RDB\0\0"
ENDIAN_MARKER = 0x12345678
PACK_VERSION = 3

# FUN_800602E4 -> FUN_80060094 uses the u16 player-record field at +4 as
# an index into 391-entry, column-major 1995/96 regular-season arrays.
REGULAR_STATS_COUNT = 391
REGULAR_STATS_BASE = 0x8AA44       # runtime DAT_8009FA44
REGULAR_STAT_COLUMNS = {
    "fga": (0x0000, "H"), "fgm": (0x030E, "H"),
    "three_pa": (0x061C, "H"), "three_pm": (0x092A, "H"),
    "fta": (0x0C38, "H"), "ftm": (0x0F46, "H"),
    "blocks": (0x1254, "H"), "offensive_rebounds": (0x1562, "H"),
    "assists": (0x1870, "H"), "defensive_rebounds": (0x1B7E, "H"),
    "minutes": (0x1E8C, "H"), "fouls": (0x219A, "H"),
    "steals": (0x24A8, "B"), "games_played": (0x262F, "B"),
    "games_started": (0x27B6, "B"), "ejections": (0x293D, "B"),
}

# FUN_80060360 uses the u8 player-record field at +6 for the compact
# 180-entry 1995/96 postseason arrays.
POSTSEASON_STATS_COUNT = 180
POSTSEASON_STATS_BASE = 0x8D508     # runtime DAT_800A2508
POSTSEASON_STAT_COLUMNS = {
    "minutes": (0x0000, "H"), "fga": (0x0168, "H"),
    "fgm": (0x02D0, "B"), "three_pa": (0x0384, "B"),
    "three_pm": (0x0438, "B"), "fta": (0x04EC, "B"),
    "ftm": (0x05A0, "B"), "blocks": (0x0654, "B"),
    "offensive_rebounds": (0x0708, "B"), "assists": (0x07BC, "B"),
    "fouls": (0x0870, "B"), "steals": (0x0924, "B"),
    "defensive_rebounds": (0x09D8, "B"), "games_played": (0x0A8C, "B"),
    "games_started": (0x0B40, "B"), "ejections": (0x0BF4, "B"),
}

# Twelve normalized u16 totals, four original u8 totals, then validity.
STAT_LINE = struct.Struct("<12H5B")
PLAYER_RECORD = struct.Struct("<HHH6B H 17B 10B 12H5B 12H5B 7I")
TEAM_RECORD = struct.Struct("<HH5I15h20B")


def stat_line(data: bytes, base: int, columns: dict[str, tuple[int, str]],
              index: int, count: int) -> tuple[int, ...]:
    if index < 0 or index >= count:
        raise ValueError(f"stat index {index} outside {count}-entry table")
    if index == 0:
        return (0,) * 16 + (0,)

    def read(name: str) -> int:
        offset, kind = columns[name]
        stride = struct.calcsize("<" + kind)
        return struct.unpack_from("<" + kind, data, base + offset + index * stride)[0]

    result = (
        read("fga"), read("fgm"), read("three_pa"), read("three_pm"),
        read("fta"), read("ftm"), read("minutes"),
        read("offensive_rebounds"), read("defensive_rebounds"),
        read("assists"), read("fouls"), read("blocks"), read("steals"),
        read("games_played"), read("games_started"), read("ejections"), 1)
    if result[1] > result[0] or result[3] > result[2] or result[5] > result[4]:
        raise ValueError(f"impossible shooting totals at stat index {index}")
    return result


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


def indexed_pointer_string(data: bytes, table: int, index: int) -> str:
    pointer_at = table + index * 4
    if pointer_at < 0 or pointer_at + 4 > len(data):
        raise ValueError(f"pointer-table index {index} is outside FEONLY")
    return pointer_string(data, struct.unpack_from("<I", data, pointer_at)[0])


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
        player_id, school_index, regular_stats_index = struct.unpack_from("<HHH", prefix)
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

        regular = stat_line(data, REGULAR_STATS_BASE, REGULAR_STAT_COLUMNS,
                            regular_stats_index, REGULAR_STATS_COUNT)
        postseason_stats_index = prefix[6]
        postseason = stat_line(data, POSTSEASON_STATS_BASE, POSTSEASON_STAT_COLUMNS,
                               postseason_stats_index, POSTSEASON_STATS_COUNT)
        school_name = strings.add(indexed_pointer_string(
            data, SCHOOL_POINTER_TABLE, school_index))
        acquisition_code = prefix[37]
        acquisition_index = acquisition_code - ACQUISITION_CODE_BASE
        if acquisition_index < 0 or acquisition_index >= ACQUISITION_METHOD_COUNT:
            raise ValueError(f"unknown acquisition code {acquisition_code} for player {player_id}")
        acquisition_method = strings.add(indexed_pointer_string(
            data, ACQUISITION_POINTER_TABLE, acquisition_index))
        players.extend(PLAYER_RECORD.pack(
            player_id, school_index, regular_stats_index,
            *prefix[6:12], struct.unpack_from("<H", prefix, 12)[0],
            *prefix[14:31], *prefix[31:41],
            *regular, *postseason, *text, school_name, acquisition_method))
        if at <= start:
            raise ValueError("player parser made no progress")

    teams = bytearray()
    assigned_players: set[int] = set()
    for team_id in range(TEAM_COUNT):
        offset = TEAM_OFFSET + team_id * TEAM_STRIDE
        roster_offset = TEAM_ROSTER_OFFSET + team_id * TEAM_ROSTER_SLOTS * 2
        roster = list(struct.unpack_from("<15h", data, roster_offset))
        try:
            roster_count = roster.index(-1)
        except ValueError:
            roster_count = TEAM_ROSTER_SLOTS
        if any(player_id != -1 for player_id in roster[roster_count:]):
            raise ValueError(f"team {team_id} roster has a player after its terminator")
        for player_id in roster[:roster_count]:
            if player_id < 0 or player_id >= PLAYER_COUNT:
                raise ValueError(f"team {team_id} has invalid player id {player_id}")
            if player_id in assigned_players:
                raise ValueError(f"player {player_id} is assigned to multiple teams")
            assigned_players.add(player_id)
        names = [strings.add(pointer_string(data, struct.unpack_from(
            "<I", data, offset + field)[0])) for field in (64, 68, 72, 76, 80)]
        teams.extend(TEAM_RECORD.pack(
            team_id, roster_count, *names, *roster, *data[offset + 84:offset + 104]))

    if len(assigned_players) != 362:
        raise ValueError(f"expected 362 assigned players, recovered {len(assigned_players)}")

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
    return (struct.pack("<8sIIII", MAGIC, PACK_VERSION, ENDIAN_MARKER, len(sections), offset) +
            directory + payload)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("feonly", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pack = build_pack(args.feonly.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(pack)
    print(f"roster pack: 29 teams, 493 players, 362 assigned, 131 free agents -> {args.output}")


if __name__ == "__main__":
    main()
