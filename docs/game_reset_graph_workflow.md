# GAMEONLY ResetGraph recovery

`src/recovered/game_reset_graph.c` owns the complete 86-instruction GAMEONLY
routine `0x80099058..0x800991AF`, called from `0x80029A20` with mode `3` during
the post-Team-Select game-entry sequence. The retail string at `0x80028204`
identifies it as PsyQ `ResetGraph`; the recovered owner is composed into
`nba97_game_main` rather than substituted by a successful no-op.

The mode-3 path emits the original seven synchronous child boundaries in
source order: diagnostic `0x8009CB2C`, byte fill `0x8009BD78`, ResetCallback
`0x800985DC`, BIOS A0:49 wrapper `0x8009BDA4`, low-level GPU reset
`0x8009B878`, and two more byte fills. With the retail fixture it publishes GPU
type `0`, width limit `1024`, and height limit `512` at `0x800C55C0`, while the
two environment-cache ranges become `0xFF`. Device and BIOS behavior stays an
explicit host boundary; the routine does not directly modify native pixels.

Compatibility deliberately retains source-era bugs and quirks. The argument
aliases through `mode & 7`; the low-level result truncates to one byte; that
byte indexes the width and height tables without a bounds check; and driver
function pointers are called without null or range guards. Unit coverage uses
reset type `255` to prove the unchecked address calculation is preserved, and
also covers the masked-mode aliases, raw unmasked debug argument, callback
mutation, unknown bytes, resource failures, and every operation-budget stop.

Private evidence under `.local/verification/native_completion/game_reset_graph`
binds the owner to source SHA-256
`a9bedc2640eb8e34716eeddeafd3c363c5961321e2f042d2f2077ed047bd6c9f`.
The bounded R3000 oracle covers all 86 original PCs across initialization,
quiet-driver, debug-driver, mode-alias, and unchecked-index cases. The visual
workflow in `scripts/verify_game_entry_visual.ps1` drives Setup, Team Select,
and User Setup through native recovered-input handlers, captures 98 PPM frames
and logs, and records the ResetGraph state receipt. It remains a deterministic
native diagnostic, not a claim that a live PS1 GPU or gameplay court ran.
