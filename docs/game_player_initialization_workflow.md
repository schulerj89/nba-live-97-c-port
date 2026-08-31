# Period player and animation initialization

`recovered/game_player_initialization.c` implements the complete direct effects
of GAMEONLY `80065B18` and its actual `56FFC(entity,1)` animation-reset path.
The two channel resetters and motion setters execute in native C; they are not
stubbed callbacks. General force0 resets, independent motion-setter calls,
sampling, GTE transforms and the rest of period initialization remain separate.

## Caller and data ownership

`65DB0` first runs `646A8`, then clock/status and `65140` lineup work. It calls
`65B18(home)` at `661D8` and `65B18(away)` at `661E8`, initializes the ball, then
calls `653E8` at `66274`. These ordering boundaries must remain intact. In
particular, the source player byte+D is read through each entity's existing
word+20 binding. Do not replace it with byte+D from a guessed starting lineup:
`65140`/`65070`/`649D8` can change the actual bindings before this call.

The native input carries side word, signed period, direction, special-center
argument, duration, prior team cumulative/sum fields, five signed formation
triples, five resolved actual player+D bytes, prior animation state, and the
two actual normalized motion0 header views. Source formation tables B891C and
B893C remain private data; the C implementation does not embed them.

The output describes only source-written fields. Team header byte values have
a write mask. Every player entity byte has a value, write mask and known mask.
The offsets document actual source field ownership; they are not a newly
initialized full PS1 record or a game runtime. Unwritten bytes have no effect.
Written bytes without known provenance remain explicitly unresolved, never
permission to write zero. `unresolved_written_bytes` counts that condition.
FDCC0 registrations use stable native entity indices and source table slots.

| Area | Effects |
|---|---|
| Team +48 | Duration on period0, otherwise previous+duration modulo32 bits |
| Team +52..5A | Five FFFF halfwords |
| Team +C0/+C2 | 0708 /0002 halfwords |
| Team +A2/+70/+30 | Zero halfwords |
| Team +B4 | Previous value plus header+32, modulo16 bits |
| Entity identity/control | Word+0, claim+4=FFFF, index+6, side byte+D9 |
| Entity positions | Signed formation coordinate transforms, scaled by source SLL8 into +8/+C; height+10=0 |
| Entity facing | Formation angle transforms, low3 bits times128 into +A2/+A6/+A8 |
| Entity reset fields | Exact byte/halfword writes at +14/+16/+18/+4E/+98/+9C/+9E/+A0/+B4/+B6/+B8/+CE/+DD/+DE/+DF/+E4/+E6 |
| Entity status | +9A=3 for nonzero player+D, otherwise0, after animation reset |
| Local center/noncenter | +1A=4/2 and +BE=0/0050 |
| Animation state | Exact force1 reset/setter writes, with prior frame/time provenance |

No player/status pointer binding (+20/+1C), opponent index/reference (+D6/+CC),
controller selection or arbitrary remaining header/entity bytes are changed.

## Animation path and retained source quirks

The reset input represents sixteen source halfwords individually with a known
bitmask. Unknown payloads are canonical zero only as native metadata; they do
not claim the original halfword was zero. Height has separate provenance.
`56FFC(force1)` calls secondary-channel `56F5C`/`56AA4` before primary-channel
`56EBC`/`5699C`. Both use motion37 for nonzero height, otherwise the current
default motion at entity+4E. In `65B18`, actual preceding writes establish
height0 and default motion0, so both supplied views must resolve directory0.

A header view includes normalized flags(+0), byte+2 and count(+7). Byte+2 is
read from the retained immutable resource; the resolver's flags/count index
does not supply it. Never infer it from channel count or manufacture a header
for an absent directory entry. Both actual headers are required by this path.

The source clears current clip/lock/flag fields, selects the new clip, clears
status bits8 then4, and sets pending/timing-control fields. However, forcing a
reset does **not** necessarily rewind a channel: when the new header flag1 is
clear and the old unsigned frame is less than the new count, its frame and
timing remain unchanged. The implementation comments and tests preserve this.
When flag1 is set or the frame is out of range, the frame/timing reset to zero.
Count0 proves any unsigned frame is out of range, even when its value remains
unknown to the native adapter.

For primary header byte+2 equal2 and flag1 clear, the source copies the newly
processed secondary frame/time into the primary fields. It does not clamp the
copied frame to the primary count. Unequal counts can therefore produce an
out-of-range primary frame; this source bug is preserved, not repaired. Unknown
secondary timing propagates to an explicitly unknown primary timing write.

An unknown frame needed for a nonzero-count comparison returns UNRESOLVED
atomically. Unknown values merely retained or copied preserve their provenance.
Unknown status+9A remains unknown under masking; `65B18` subsequently writes an
actual known0/3 from player+D, resolving that field. No previous frame counters
are invented. At a fresh original game entry, a separately implemented and
verified `2DB90` clear of FDB4C lengthE7C can establish initial entity zeros;
it must not be repeated as a convenient reset for every period or match path.

## Native guards and integration

The native side range0..5 ensures each five-record span remains in the owned
ten-player storage. Ordinary source callers use0 and5; nonordinary bounded
spans are still supported for diagnostics. Other raw source side words would
leave this owned storage and return STORAGE rather than inventing entities.
Missing motion views, mismatched directory indices, invalid provenance and
needed unknown inputs return explicit errors without modifying output.

Period0 does not need a prior cumulative+48 value because the source replaces
it. Other periods do. The +B4/+32 sum needs real prior operands. Prior selected
animation fields may stay unknown when the actual source path replaces them;
invalid token encodings still fail validation. Input/output overlap is safe.

To integrate, add the C file and its dedicated CTest target (C99/C++17, `src`
include directory). After existing player bindings and lineup changes, resolve
each entity's actual player+D, capture owned animation fields and actual motion
header views, and invoke the home/away initializers in order. Apply only written
known fields and preserve unresolved fields visibly; a concrete simulation must
resolve any fields it needs before using them. Register each completed entity
at its FDCC0 slot, then continue the source ball and controller-selection setup.
Do not call this projection a completed period or enable a match from it alone.

## Verification

Fresh private differential checks passed2,800 `65B18` cases and2,200 standalone
`56FFC(force1)` cases. The original instructions execute both resetters and both
setters without callee hooks. Exact written-byte footprints and8,600,800 final
bytes, including untouched header/entity bytes, agree with the native effects.

| Source owner | Full instruction denominator | Unique executed in these force1 comparisons |
|---|---:|---:|
|65B18|166|166|
|56FFC|16|16|
|56F5C|40|38|
|56EBC|40|38|
|56AA4|53|51|
|5699C|66|64|

Full denominators remain visible. This does not claim force0 or independent
setter branches are implemented merely because most instructions execute on
the scoped path.

An additional12 comparisons use actual privately extracted ZMOCAP normalized by
original640D8 and original formation tables, across home/away, periods0/2/4 and
cold/warm synthetic animation state. The `65B18` chain remains unhooked. These
are resource-backed synthetic checks, not natural original runtime captures.

Public tests passed MSVC `/Od` and `/O2`, both `/W4 /WX`, with196,608 count/mode
combinations each plus field ownership, coordinate/angle transforms, wrap,
source quirks, missing-header guards, unknown provenance and overlap tests.

Reproduction and receipts remain under ignored
`.local/verification/native_completion/player_initialization/`:
`verify_initialization.py`, `verify_actual_motion.py`, `verification.json`,
`actual_motion_verification.json`, and `build_logs.json`. No original asset,
formation table, binary code or generated recompiler material is public.
Native host integration, original live-runtime comparisons and gameplay remain
outside these receipts.
