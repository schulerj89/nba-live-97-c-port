# Video-mode query recovery

`game_video_mode.c` owns the complete GAMEONLY routine
`0x800985CC..0x800985DB`: 16 bytes and four instructions. The fresh Ghidra
listing `game_800985cc.txt` has instruction SHA-256
`6573858a77338ffd6166fbc44753517ce124fb50c76eeae7fc9e1d7540013d32`.
Known callers are `0x80099DE8`, `0x8009A034`, and `0x8009CA2C`. The routine
has no children.

The owner forms `0x800C0000` in V0, loads the raw word at `0x800C54AC`, and
returns through live RA after a NOP delay. The value is not normalized to a
Boolean or video-mode enum: values such as 2, 255, `0x80000000`, and
`0xFFFFFFFF` remain exact. Partial source bytes also return with their exact
knownness when RA is known. Unknown RA stops only after the full read and NOP.
Every other GPR, SP, HI/LO, and guest byte remains unchanged.

The C99 owner reads the fixed PS1 address through validated little-endian
retained memory. Budget zero preserves the known `0x800C0000` LUI prefix.
Unmapped or malformed loads preserve the same prefix, and a malformed later
knownness byte cannot partially replace V0. An optional journal exposes the
single read PC, address, width, value, mask, and operation without parsing log
text.

`game_video_mode_adapter.cpp` binds BE to both recovered BA display sites:
`0x80099DE8` with RA `0x80099DF0`, and `0x8009A034` with RA `0x8009A03C`.
Both require the exact delay PC, `0x800985CC` entry, video-mode kind, and zero
arguments. The adapter copies all 32 GPR words and masks plus HI/LO field by
field between BA and BE, retains separate budget/journal/progress slots for
both calls, and routes every other BA child through its typed fallback.
Invalid incoming metadata, machines, memory, or journals leave BA's machine
unchanged.

The focused executable performs 721 always-active checks. It covers raw values
0, 1, 2, 255, `0xFFFFFFFF`, and `0x80000000`; all 16 source knownness masks;
all 32 GPRs plus HI/LO; the exact budget-zero LUI prefix and budget-one load;
unknown RA after the read; unmapped and malformed source data; invalid,
overlapping, empty, overflowing, and null regions; implicit knownness;
truncated and invalid journals; no guest writes; null arguments; and
deterministic machine state.

The natural executable performs 273 always-active checks through the actual BA
display owner. An unchanged cache skips both queries. Changed rectangle and
mode state reaches both exact call sites. Raw values 0, 1, and 2 drive distinct
source branch outcomes without adapter normalization. A partial video word
preserves BA's signed-halfword read at `0x80099DF0`, byte store at
`0x80099DF4`, and subsequent unknown branch prefix at `0x80099E10`. It also
covers each site's independent budget, exact metadata and RA, fieldwise machine
preservation, kind-or-entry rejection, invalid incoming state, and fallback
routing. Both tests compile under strict C99/C++17 with `-Wall -Wextra -Werror
-pedantic-errors` and use only runtime-generated data.

The independent original-instruction differential passed 4,096 cases across
all four PCs, all 32 GPRs plus HI/LO and their masks, all 16 source masks,
budgets zero and one, unknown RA, and unchanged mapped memory.

Production dependencies are the shared retained-memory/full-machine types and
the recovered BA display event. The third caller at `0x8009CA2C` and BA's
remaining typed GPU/copy children remain outside BE.

Gameplay shown: **NO - no direct visual effect**. The routine only reads a
retained CPU video-state word. Natural evidence proves the resulting BA state
and memory prefixes; BE itself does not issue display commands or render
pixels.

Manager native integration composes the actual scene startup, both display
environment calls, this query at both source sites, and the BIOS copy
trampoline. The first environment matches its cache and skips the query; the
second differs and reads the runtime video word twice. GPU command consumption
and BIOS byte transfer remain explicit synthetic services. The capture checks
the five resulting GP1 commands and the copied environment video byte.
Independent original comparison passed 4,096 cases across all four instructions,
including full machine state, all 16 source byte masks and unknown return PCs.

Manager verification passed 721 focused checks, 273 natural-caller checks and
all 303 asset-free CTests. Strict C99 and progress/recovery/instruction/roster
freshness checks passed. Native run
`.local/verification/team_select/game-entry-20260906-025939-a6eb0c3b`
recorded 98 scripted states and both one-read query invocations returning the
raw word 1 with mask 15. Both CPU-only frame hashes were
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
User Setup remained displayed; the advancing native match is still pending.
