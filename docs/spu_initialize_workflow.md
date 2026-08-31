# Original sound initialization

`recovered/spu_initialize.c` expresses five original FEONLY routines as portable
C: `7E6EC` (8 instructions), `7E3FC` (50), `7CE18` (327), `7F5D0` (12), and
`7DD80` (18). These 415 source instructions are independently compared with the
retained original bytes. This is ordinary native code, not a runtime instruction
interpreter. GAMEONLY addresses are a different namespace.

`7E6EC` selects mode zero. `7E3FC` first calls the live controller initializer
through `C7DC4 + 0C`, then initializes the sound hardware. In mode zero it writes
24 software voice halfwords in descending address order, from `C7676` to `C7648`.
It then runs the existing SPU event initializer, reads the live reverb address,
clears selected software fields, writes the reverb register through `7DD80`, and
clears three final globals. It does not initialize unrelated bytes or supply an
event handle itself. Its raw return is the address returned by `7DD80`.

`7CE18` changes DPCR through its actual source pointer, clears master volumes
and control, waits for the low eleven status bits, establishes the source unit
and alignment globals, and issues the original register writes. Mode zero also
calls the existing `7D334` PIO routine with `C7604` and a count of 16, resets six
registers for each of 24 voices, and issues the original key commands. The
original initial bytes at `C7604` are sixteen `07` bytes; they are not a fabricated
zero block. The PIO owner reads the actual retained bytes at execution time.
Nonzero mode skips this PIO/voice section but still performs the earlier setup
and final writes.

Original behavior retained and commented:

- The status poll increments the live `C75C0` counter. At 5001 it prints its
  diagnostic and continues initialization. Neither a timeout nor an ignored
  lower return is replaced with a new source error.
- Source pointers are cached or reloaded at the exact original accesses. Device
  callbacks can change retained globals; subsequent reads see those changes.
- The low key-on and key-off reads occur even though the next instruction
  discards each value and writes `FFFF`. Upper-half writes retain their original
  read/OR sequence.
- `7DD80` does not bound its index. Address arithmetic wraps at 32 bits, the
  optional right shift masks its count to five bits, and the returned value is
  the computed address rather than the written halfword.
- The controller target remains live. A completed nonlocal controller return
  ends the entire outer initialization; sound setup does not resume after it.
- Private stack delay loops do not establish a portable hardware clock. No
  invented sleep, elapsed-time claim or synthesized status replaces them.

The callback journal distinguishes completed operations, refused imports and
completed nonlocal transfers. Unknown unused raw returns stay unknown. Access
and journal limits retain their completed prefix; they are not resumable
checkpoints and do not authorize repeating effects. Active source stack/code
aliases and aliases between mapped RAM and native owner metadata are outside
the API contract.

## Native composition

`spu_initialize_backend.cpp` connects the live `7F708` controller target to
`InterruptControllerBackend::run`, event initialization to recovered `7E4C4`,
and PIO to recovered `7D334`. It borrows the same native controller, event registry
and sample storage used by later uploads. Separate lower journals share a
cumulative lower-owner access budget. The outer initialization retains its own
budget and journal, including direct device imports. Malformed bindings and mismatched owners or memory
generations refuse before executing lower operations. Other controller targets
and diagnostics require an actual external callback.

Request-producing sample writes use the CPU allocation generation, which can differ from
the event registry's generation. This also applies when the unchecked register
setter reaches FIFO or DMA registers through a live index. Other writes retain
the shared controller/event route and its device/RAM overlap checks. A refused
sample operation is not replayed in an external fallback.

The sample backend retains FIFO halfwords and copies them into its owned sample
storage only at an explicit PIO service call. Startup's 16-byte transfer is
within its supported single-chunk domain. It neither pads that transfer to 64
bytes nor delivers a DMA completion event. Fresh address, transfer mode,
generation, range and ownership requirements remain explicit. Unproved wrapping
or continuation into another FIFO chunk is refused, not silently truncated.

The backend retains the exact startup configuration writes separately from
hardware readback. Key commands, status observations and physical timing are
not inferred from this retained write state. An unknown canonical SPU status
read requires an explicit platform observation; unknown CPU RAM and other
unknown registers do not gain a permissive fallback.

The integration test now runs original sound initialization before the original
heap initializer and the existing program/PATl/transfer path. Its PIO service
point, status observations, key-command readbacks, interrupt hardware and BIOS
counter policies are explicit test conditions. Subsequent DMA completion still
travels through the recovered controller handler, DMA dispatcher, registered
SPU ISR and native event registry. This proves source ordering and native byte
ownership under those conditions, not physical PS1 timing or natural host boot.

The larger game entry `700B0` also calls the common attribute setter, music
initialization and timer callback registration. Those remaining producers,
resource allocation/padding, synthesis, real frame scheduling and full playable
matches are not established by this initialization module.
