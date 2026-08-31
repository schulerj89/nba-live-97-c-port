# Court startup sequence source owner

`nba97_game_court_startup_sequence` is the canonical C99 composition owner for
GAME `479B8..48D5C` (1,257 original instructions). It invokes the already
recovered children in source order: roster `479B8..47CB8`, interactive
`47CB8..484B8`, packet startup `484B8..48744`, texture selection
`48744..487B8`, texture upload `487B8..48894`, geometry selection/sync/free
`48894..48A4C`, resources `48A4C..48D28`, then the private 13-instruction ABI
epilogue `48D28..48D5C`.

The context exposes one canonical `Nba97GameTextMemory`. Each CPU child receives
that exact value so mutations, knownness, and native backing aliases remain
live across boundaries. Budgets, event journals, child progress, and IO owners
remain separately typed; the composer neither merges their accounting nor
relabels child return codes. On refusal it returns the exact child status and
retains every completed earlier interval plus the reached child's prefix.
Later intervals are not preflighted and there is no default-success backend.

The loader's raw texture result is not converted from a host pointer. The pure
allocation-registry resolver requires one exact raw-address match, rejects an
ambiguous duplicate, and checks that the retained image reference's independent
alignment provenance agrees with the original address. It does not read,
validate, copy, or mutate payload bytes. The existing image owner performs
reached payload/knownness checks. The geometry loader's raw result passes
unchanged to the existing court-resource owner.

The final 13 instructions restore `ra`, `fp`, `s0..s7`, adjust the private
source stack, and return. Source stack/register storage is outside every public
retained owner, so the composer records completion without inventing a visible
memory event.

Verification binds the public C object together with the real child owners and
checks a complete zero-group source path, exact service ordering and handoffs,
typed child completion, allocation resolution, no later-stage preflight, and
prefix retention at interactive, registry, and allocator stops. The private
oracle proof executes the unmodified original CPU body, covers all 1,257 PCs,
and compares the corresponding native source-order path and retained effects.

This closes a callable `479B8` body only. The real outer `52C20` selection,
producer state, loader/heap/device services, and match-flow connection are not
part of this owner. Accordingly `progress.natural_entry` is always false; this
is not evidence of natural startup, first possession, screenshots, or gameplay.
