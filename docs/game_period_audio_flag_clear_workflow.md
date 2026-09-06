# GAMEONLY period audio flag clear recovery

`nba97_game_period_audio_flag_clear` owns GAMEONLY
`0x8002A244..0x8002A253` inclusive: 16 bytes and four instructions. Fresh
Ghidra evidence is recorded in `game_8002a244.txt`, with instruction SHA-256
`3f92cb180bea44bc3125f0b4f19228ba4a282f8d4a48af628af3a2c20c4786af`.
Its sole caller is the recovered first-period startup owner at `0x80067400`;
the routine has no callees.

The C99 owner receives the full 32-GPR/HI-LO machine, mapped retained memory,
an operation budget, and an access journal. The LUI sets `at=0x800B0000`, the
single SB writes a known zero to `0x800B1FD5`, and the JR NOP completes through
the live `ra`. Every other machine word and byte mask is preserved. Budget
failure occurs after the LUI prefix and before the store. Unknown or misaligned
`ra` is refused only after the store and JR delay slot, retaining that completed
memory prefix.

The production adapter claims any first-period event carrying the assigned PC,
delay PC, entry, kind, or known return address before fallback. It accepts only
the natural operation-three event at `0x80067400`, delay `0x80067404`, entry
`0x8002A244`, zero arguments, and `ra=0x80067408`. The parent supplies only GPR
state, so HI and LO enter the owner explicitly unknown. The wrapper runs the
real first-period owner and leaves all remaining children behind its existing
typed callback.

The focused test passes 9,511 checks. It covers all 256 prior flag bytes, the
exact store journal and neighboring bytes, varied masks for every untouched
GPR and HI/LO, budget zero, unknown and malformed destination knownness,
knownness-free mapped storage, journal truncation, empty and malformed maps,
`SIZE_MAX` and address overflow, overlap, exact KSEG mapping, all fifteen
partial `ra` masks, three known misaligned returns, invalid machine metadata,
null arguments, and deterministic memory/knownness/machine repetition. The
36-check natural test executes both first-period presentation paths, proves the
incoming `v0` word and mask reach this owner unchanged, checks exact event
guards and fallback, reuses one binding and RAM, exercises owner budget failure,
and removes only the audio-flag byte from an otherwise valid parent mapping to
prove the natural resource prefix. Every fixture is generated on the heap at
runtime and contains no retail assets.

Strict Clang C99 with `-pedantic-errors -Wall -Wextra -Werror`, strict Clang
C++17, and MSVC C11/C++17 `/W4 /WX` compile cleanly. Both focused and natural
executables pass. Manager integration needs the new C owner and C++ adapter
plus the existing `game_first_period_startup` owner; no shared build file is
changed here.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
routine clears one retained CPU-side audio flag and performs no rendering or
game-state advancement.

Manager review independently decoded the original four instructions and
compared 8,192 cases, all 34 machine words and known masks, all original byte
values, budget 0/1, and unknown/aligned/misaligned return prefixes. The ignored
receipt is `period_audio_flag_clear_differential.json`.

Integration passed all 349 asset-free Debug CTests (5.32 seconds), progress and
C recovery validation, instruction-semantics freshness, and roster checks.
Native input run `game-entry-20260906-064343-e6514850` executes the owner from
the real first-period caller in both presentation cases on shared diagnostic
RAM. Explicit flag 215 becomes zero in one store at 0x8002A248. The music
owner's incoming V0=1 is preserved, AT becomes 0x800B0000, RA is 0x80067408,
and SP is 0x801FFED0. Unavailable parent HI/LO remain explicitly unknown.
Before/after CPU frames share SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed frontend is still User Setup; no advancing match is shown.
