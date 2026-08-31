# Original SPU transfer and completion CPU owners

`src/recovered/spu_transfer.c` recovers six complete FEONLY CPU functions:
`7DC90` (34 instructions), `7D9E8` (170), `7D334` (205), `7F568` (3),
`7D668` (104), and `7F508` (3). These 519 instructions cover transfer
selection, DMA register programming, programmed-I/O transfer, interrupt
completion, and the TestEvent/DeliverEvent BIOS thunks. Required device,
diagnostic, callback and BIOS effects remain explicit platform operations.
Executing this CPU owner alone neither copies DMA samples nor makes sound.

## Preserved behavior

The wrapper reads the live transfer mode. DMA mode programs the address,
selects write mode, and issues the transfer in that order. It ignores lower
raw timeout returns and returns the original requested size. A native refusal
is different: it stops at the reached unowned operation and retains earlier
effects. It must not be translated into a successful source return.

DMA rounds requests up to 64-byte blocks. The complete block count is stored
in the retained global before the register write shifts it left by 16 bits,
truncating high bits. No clamp, zero padding, or replacement with the logical
request size is inserted. Address conversion uses the live shift and retains
the original low-16-bit store. The platform must honor the actual registers
within its supported domain or explicitly refuse unsupported hardware cases.

Programmed I/O reads halfwords even for an odd final count, requiring the byte
after the logical end. Its busy timeout prints through required `83B20` and
continues to the next chunk. Its final status timeout returns the diagnostic's
actual raw result, which can be explicitly unknown. The outer transfer wrapper
still ignores that result. Native byte ownership and knownness checks do not
silently repair any of these original behaviors.

Interrupt completion clears the control mode and polls. Even a source poll
timeout proceeds to completion handling. A nonzero live `C75FC` invokes that
callback instead of delivering the default event. The callback is read again
before invocation. Missing callbacks refuse; they do not become successful
default event delivery. The callback has no declared arguments; the journal
records the observed `a0` residue and documents unavailable argument state.

The private stack arithmetic delay loops establish neither native elapsed
time nor an invented device tick. Hardware cadence, cancellation, competing
sample writers, and active source code/stack aliases are outside this owner.

## Completion and startup obligations

Transfer submission, copying bytes, interrupt dispatch, event delivery, and
event consumption are separate effects. A platform must not use WinMM
playback activity or a DMA-start flag as a successful TestEvent result.

The original startup `7E4C4` registers `7D668`, opens a noninterrupt event for
class `F0000009` and spec `20`, stores the returned handle at `C7678`, and
enables it. Its guard is written before those operations, without rollback.
The actual DMA callback registration depends on earlier interrupt-controller
initialization. Calling the recovered transfer or interrupt function does not
claim that these startup owners have executed.

The source does not clear a pending BIOS event when starting DMA. A stale
event may therefore be consumed during a later transfer. Sony's SDK describes
TestEvent as consuming an enabled noninterrupt event's pending state; repeated
polling cannot stay permanently true from one delivery. See the
[Sony Run-Time Library Overview 4.6](https://psx.arthus.net/sdk/Psy-Q/DOCS/LIBOVR46.PDF),
printed page 2-11. This corroborates the platform event contract, not a claim
that the port executes an original BIOS ROM.

## Native contract and verification

All CPU RAM accesses use the caller's retained `Nba97VoicePatlMemory` registry.
Device events identify the source's reached pointer accesses, rather than a
guessed device address range. A live pointer alias into owned CPU data must be
resolved against the same registry. Canonical flags, byte knownness, alignment,
and unambiguous ownership remain required. Device reads must return known
bits. Unused callback results may remain unknown, but malformed knownness
metadata always refuses after recording the completed callback prefix.

Access budgets include RAM reads and writes and required platform calls.
The journal records stores and platform operations, including a refused call
as incomplete. Journal and access limits retain the executed prefix. There is
no automatic rollback or resumability. Caller configuration, journal, progress
and mapped data cannot alias or change metadata during execution.

Public CPU tests use explicitly scripted registers and event fixtures; those
fixtures are not a sample-memory backend. They check original timeout and
rounding quirks, odd-halfword ownership, unknown diagnostic returns, callback
versus delivery, consuming polls, and exact bounded prefixes. Independent
verification executes the original FE instructions and compares ordered
effects, retained bytes and annotations, returns, and refusal locations.
Instruction coverage proves the recovered CPU paths within the declared
platform contract, not unmodeled hardware timing or full audio startup.
