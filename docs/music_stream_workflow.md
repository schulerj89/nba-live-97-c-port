# Finite music stream and native output contract

`src/recovered/music_stream.c` closes the original staging and logical drain
boundaries needed by a native five-track player. It does not add a second audio
decoder or a speculative player. `music_voice.c` retains voice/fade/completion
control; `music_routing.c` retains resource selection. The host can replace CD
reads, ADPCM transfer, IRQ delivery and output storage with native operations
while preserving the recovered decisions below.

Source: private FEONLY.BIN SHA256
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
Addresses omit the80000000 runtime prefix. Private original instructions and
receipts are in `.local/verification/native_completion/music_stream/`.

Why ordinary music is finite

The caller2F36C supplies fifth argument0 to28C50. That wrapper forwards it to
6A8F8, which stores its low byte in C6CA9. Stream load6C93C first releases any
queued continuations at stream+74 and then creates one selected-resource
request.6D158's phase7 pops that request; its next link controls whether another
resource follows. The caller does not schedule a shuffled playlist here.

The chunk dispatcher6B064 sends SCEl to6BB48. With C6CA9==0,6BB48 writes
FFFFFFFF to the queue tail's next link, then releases the SCEl record through
6F1FC. That release marks its record FFFFFFFE. When6BBB8 reaches a head equal
to the tail whose next link is FFFFFFFF, it releases the tail and returns both
data pointer and byte count0.726C4 preserves that null pair;72254 subsequently
sets producer-ended C6D29. The ordinary stream therefore has an explicit end
without deriving it from a PCM cursor or waiting for a routing deadline.

Continuation mode is different. A nonzero C6CA9 does not close the tail on
SCEl. If the consumer reaches an open tail,6BBB8 can call71BC4 to replace its
payload with ADPCM silence and mark its tag FFFFFFFF; this is starvation,
not natural end.6D158 also skips the SCEl handoff at6DBC0 when another resource
request is queued. All five audited files contain SCHl, SCCl, SCDl and SCEl;
none contains the SCLl loop control dispatched through6BAD4/6CA68. Do not add
an unconditional loop or apply ordinary finite behavior to an unclosed
continuation stream.

Recovered implementation

| Entry point | Original scope | Host effect boundary |
|---|---|---|
| `nba97_music_stream_next` |726C4 metadata and cursor control | Fetch node metadata and source token |
| `nba97_music_stream_fill` |72254 staging control | Copy source ADPCM bytes to channel staging regions |
| `nba97_music_stream_end` |6BB48 tail-link decision | Release the SCEl record after mutation |
| `nba97_music_stream_irq_stop` |7313C..7319C and734C4..73554 stop arms | Queue key-off; no immediate FINISHED |
| `nba97_music_stream_irq_advance` |7333C..73424 read advance | Program next slot's IRQ address |

`StreamBlock.tag` represents the queue node's word8 after6B838 moved the
original chunk tag there. It is not the checksum occupying word8 in the file.
`bytes` is SCDl word12; payload starts at token+16, with contiguous left then
right channel bytes. The token is an adapter address/offset, not a required
PS1 virtual pointer. The header-format field projects the SCHl tone byte read
by726C4; it is0 in all five current headers. The callback must supply complete,
validated metadata and maintain source data lifetime across copies.

726C4 skips one SCHl and fetches again; a second SCHl yields control without
copying. It tracks transitions into/out of silence and header-format changes
at the current write index. It uses signed byte-count/channel division.
72254 fills1024 ADPCM bytes per channel, preserving signed halfword counters,
wrapping arithmetic, partial staging across calls, and the17-buffer starvation
threshold. The native helpers preserve source divide-by-zero traps as negative
returns with preceding state mutations retained. They do not invent valid
channels, clamp malformed sizes, or cap a non-progressing source loop.

The original drops partial tails

72254 returns1 only when its staging buffer is full.72954 at72C38..72C60
submits a transfer only for that result. At end, an incomplete staging buffer
is retained but is not padded or submitted. The normal SPU IRQ refill path
at73470 does not request further fills once producer-ended C6D29 is set.
This source behavior must not be silently repaired by playing every frame
returned by the decoder.

Every real SCDl byte in all five files was passed through original726C4/72254
and through the native helpers. The full output blocks match byte for byte,
including concatenation across SCDl boundaries. Each full block represents
64 ADPCM frames/channel, or1792 PCM frames. These counts describe the prefix
eligible for transfer, not exact audible SPU duration:

| File | Full staging blocks | Full staging PCM frames | Declared decoder frames excluded by partial tail |
|---|---:|---:|---:|
| ZTMENU1.CNK |4141|7420672|937|
| ZTMENU2.CNK |3972|7117824|1664|
| ZTMENU3.CNK |3196|5727232|1536|
| ZTMENU4.CNK |3232|5791744|1408|
| ZTPAUSE.CNK |5196|9311232|1381|

The excluded ADPCM bytes/channel are544,960,880,816,800 respectively. These
include the small capacity padding beyond each file's declared sample count.
The public tests explicitly retain the partial-tail loss.

Logical drain is earlier than final-buffer return

6F968 allocates a ring with1024-byte channel slots: routing parameter400 gives
200 stereo slots, and parameter210 gives105. The choice is the source ED2AC
input, not the filename: random selection of ZTPAUSE with ED2AC0 still uses
200 slots. This does not establish gameplay Pause behavior.

At start,72954 programs the SPU IRQ at the ring base+8.7309C subsequently
programs each next slot at its base+8. Its stop arm runs **before** its read
counter advances. It queues key-off when stop-request C6D58 is nonzero, or
when producer-ended is nonzero and signed `read_index == write_index - 1`.
Only the explicit request is consumed. The stereo pair is added only when
channels==2. It does not set FINISHED;702B0 and the voice completion helper
still wait for hardware status0.

Whole original7309C executions with a deterministic available-data refill
model confirm key-off at IRQ ordinal N-1, with read_total N-1, for each real
track's N full staging blocks. The same holds for ZTPAUSE with105 or200 slots.
This is a logical event near the start of the last full slot, rather than a
license to play its entire PCM buffer and then finish. Exact ADPCM prefetch,
SPU IRQ latency, audio service latency, ADSR release and remaining audible
samples require hardware evidence; no exact sample cutoff is claimed here.

The signed non-modulo comparison also has an original boundary bug: if final
write_index is0, a normal nonnegative read index never equals-1. The original
200-block/200-slot scenario did not queue natural key-off through an extra
ring traversal. The native helper retains this; do not replace it with a
modulo-corrected comparison or a blanket PCM-EOF completion flag. None of the
five current staging-block counts hits that boundary for its ordinary setup.

Concrete host integration

1. Load the selected private CNK through the portable decoder and validate its
   header/chunk spans. Keep unmodified decoder output and its full frame count
   for decode verification. Source playback length is a separate property.
2. Feed SCDl metadata to `music_stream_fill` in file order, with channels2,
   staging_size1024 and initial staging_remaining0. Start cursor/tag/count
   fields at their source initializer values:7390C clears data, bytes,
   consumed/channel bytes, read/write totals, write index, staging remaining,
   header format, producer-ended and underrun-active; underrun/resume/format
   indices areFFFF. It does not clear tag or previous_tag; retain them between
   source initializations. Supply SCHl metadata for continuation handling.
   The already decoded PCM can back an output plan: each successful fill adds
   one1792-frame unit. A copy callback can validate token/span boundaries and
   account for those bytes without decoding them a second time. Preserve the
   final partial-block refusal.
3. Retain separate ring read/write counts, source voice state, output-buffer
   ownership and generation. Platform transfer completion advances the source
   write frontier. Native rendering progress supplies slot-entry events, with
   a startup event for slot0; completion of a prior PCM unit can model entry
   into the next slot. Do not count buffer submission as playback progress.
4. At a slot event call `music_stream_irq_stop` before the advance helper. If
   stopping, queue the source key-off and move through702B0's state1 lifecycle.
   Otherwise preserve any applicable starvation/format arms before advancing.
   The returned IRQ address identifies the next slot; the host need not expose
   a fake SPU register. Initial prefill/refill availability remains an adapter
   scheduling decision and must not move the logical stop test to buffer end.
5. Apply gain/fades using the recovered voice service at its proven cadence.
   After key-off, returned WinMM buffers can establish native storage safety;
   they are not original ADSR observations. If the native adapter models its
   own output drain as hardware status0, label that platform substitution
   explicitly. Do not claim original release-waveform equivalence. Route the
   resulting source completion through `music_voice_complete`, then allow the
   routing owner to retire/select the next resource.

WinMM storage must still outlive outstanding headers and callbacks, including
reset returns, old generations and source retirement deadlines. See
`music_voice_workflow.md` for the verified Microsoft ownership/callback
requirements. A finite output implementation following these boundaries is
possible now; source-equivalent SPU waveform timing is a separate remaining
verification task, not a reason to introduce a guessed loop or stall all
native multi-track integration.

Proof and remaining limits

The standalone MSVC `/O2 /W4 /WX` build and Release-safe public tests pass.
The current private oracle compares600 staging sequences,240 next-block cases,
3000 stop-arm cases,2000 read-advance cases, all256 keep-open bytes,12033 real
SCDl chunks and19737 full staging blocks.82828 callback events match with full
state snapshots, over7963961 original instructions and622 distinct addresses.
The end proof executes actual6F1FC marker writes, including the original stale
queue head/tail after release. The native staging provider does not expose
those stale source pointers as reusable host pointers.

`report.json`, `track_prefixes.json`, `source_mips.txt` and build logs record the
comparison. `drain_schedule.py`/`drain_schedule.json` separately execute whole
7309C for ten ring scenarios, including the source zero-wrap bug. That proof
uses explicit platform hooks and a refill model; it is not a hardware trace.
The full CD task machine6D158, allocation, arbitrary SCLl continuations,
starvation waveform, SPU decoding/mixing and hardware-time completion have
not been claimed as ported by these helpers. Original files and PCM remain
private; there is no XA implementation or gameplay Pause inference here.
