# GAMEONLY camera selection recovery

`nba97_game_camera_select` owns GAMEONLY `0x800799CC..0x80079D37`
(inclusive), 876 bytes / 219 instructions. The private fresh-Ghidra record is
`.local/evidence/tipoff-recovery/game_800799cc.txt`, routine SHA-256
`7ebbad49fdd6daaf0fb679d5c20123ac73e9d5b2725485e216aa6b0681ce2035`.
Its 17 known callers are `0x80067134`, `0x8006714C`, `0x800671C4`,
`0x80078AF4`, `0x800782C0`, `0x800796B8`, `0x800796E4`, `0x8007A184`,
`0x8007A6A8`, `0x8007E3A0`, `0x8007E3DC`, `0x8007E424`, `0x8007A31C`,
`0x800384BC`, `0x80038634`, `0x800677E4`, and `0x800677FC`.

The owner forms the original `0x58`-byte frame and keeps all 32 GPRs with
per-byte knownness. Mode zero samples `0x800FC99C`, clears a prior nonzero
value before calling `0x8007E26C` at `0x80079A0C`, republishes callback-live
`s0`, and returns. Nonzero modes set the busy byte at `0x801029F8` before the
signed dispatcher. Modes 8, 9, and 12 call `0x8007C964`, `0x8007CC3C`, and
`0x8007D3C8`; modes 100 through 106 call `0x8007CAF4` with `a0=mode-100` and
then force live `s0=9`; modes 200/201 and 202/203 call `0x8007A19C` with
`a0=11` or `a0=10`, while the delay slot forces `s0=10`.

The 200-through-203 arm reloads live `0x800FC99C` after its child. When that
word equals callback-live `s0`, it calls `0x8007A3A0` at `0x80079B00` and
returns without clearing `0x801029F8`. This busy-byte-one early return is an
original quirk. Every other signed or unchecked mode keeps the original branch
behavior and wrapped `0x800BC268 + mode*4` table address.

The common path publishes live `s0` to `0x800FC99C`, reads the unchecked table
entry, publishes the force flag to `0x800FA62C`, and stores the entry at
`0x800FC9D0` in source order. Mode 10 calls `0x80079EBC(0x100)`, copies
`0x800FC9A0/A4/A8` to `0x8010607C/80/84`, and calls `0x8007A19C(10)` only if
the first provider leaves live `s0` equal to ten. Other modes call
`0x80079F78` with the unsigned byte at `0x80021ED8`.

Selector `s1=1` copies all 14 words at `0x800FC99C..0x800FC9D0` to guest
`sp+0x10..sp+0x44` as three four-load/four-store batches followed by two loads
and two stores. After `0x800798B4(-1)` at `0x80079C2C`, it rebuilds its cursors
from callback-live `sp` and restores the same 14 words in the identical batch
shape; the last store executes in the `0x80079C7C` jump delay slot. Selector
zero calls `0x800798B4(-1)` at `0x80079C8C`, copies six words from
`0x80109AA8..0x80109ABC` to `0x800FC9A0..0x800FC9B4` in paired load/store
order, and clears the following six words. Other selector values skip both
copy arms. Normal completion clears the busy byte, calls `0x8007A3A0` at
`0x80079D0C`, writes `-1` to `0x800BC1F4`, and reloads `ra/s1/s0` through
callback-live `sp`.

All ten direct target addresses remain one typed unresolved callback surface.
Events identify the exact call PC, delay-slot PC, target, argument count, and
operation number; callbacks observe and may mutate all GPRs and retained
memory. The owner retains little-endian access order, alignment traps, mapping
failure, callback refusal, byte unknownness, pure arithmetic knownness, and
bounded operation prefixes without casting a guest address to a host pointer.

`nba97_game_camera_select_from_camera_startup` composes the owner with the
already recovered camera startup at its natural `0x800796B8` and `0x800796E4`
sites. It records the exact 32-GPR child-entry state, forwards the caller's
retained memory, and copies complete or partial child GPR state back to the
natural caller.

The focused runtime-generated fixtures cover signed switch boundaries and
wraparound, both mode-zero old-state arms, the busy-byte early return, live
child mutations, selectors zero/one/other, exact 14-word access order, guest
stack relocation and aliases, unknown branch delays, alignment/mapping errors,
all call-site refusals, callback-invalid state, and every operation budget with
all 32 GPRs compared. The integration fixture drives both natural startup call
sites and validates their complete full-GPR entry state. No retail data,
binary fixture, capture, or generated asset is required.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
routine is CPU-side camera orchestration through unresolved typed update
providers. The tests prove retained state, GPR, call, and access effects; they
do not claim an advancing rendered match or a pixel change.

The input-driven native game-entry diagnostic now composes this same adapter
under recovered camera startup following the recovered hot-start output. Its
explicit synthetic camera services exercise mode 12, selector zero, a live
unchecked table pointer, six copied words, six cleared words and the busy-byte
clear. The receipt proves 37 operations, 12 reads, 21 stores and four child
calls with identical diagnostic scanout hashes. The separate frontend handoff
frame remains User Setup; no live tick prologue or advancing match is claimed.
