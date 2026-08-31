# Owned sample memory and upload completion

`SpuSampleBackend` owns 512 KiB of sample bytes and a separate known-byte mask.
New storage is unknown. A transfer makes only its actual copied range known;
it does not initialize the rest of sound memory or synthesize audio. The size
is documented in Sony's [PlayStation Hardware manual](https://psx.arthus.net/sdk/Psy-Q/DOCS/Devrefs/Hardware.pdf),
section 4-4. This component is a bounded native storage and device boundary,
not an emulator or a claim of completed original hardware startup.

## Actual transfer effects

The backend recognizes the reached transfer registers and resolves retained
CPU aliases through the same `Nba97VoicePatlMemory` supplied to the recovered
CPU owner. A CPU mapping that overlaps a recognized device slot is ambiguous
and refuses. Importing entry registers is an explicit provenance operation;
it cannot create a DMA request or substitute for a fresh address-register
write. Programmed-I/O now supports the source initialization's single FIFO
chunk through explicit queueing and service, as described below.

A supported CHCR store captures a request from the actual register values.
It does not capture a host CPU pointer, copy bytes, clear an older event, or
deliver completion. `servicePendingDma` resolves the CPU storage again using
the retained allocation generation, validates the full reached span, and
performs the copy. Changes to CPU bytes before service are therefore visible.
Every rounded source byte, including padding beyond the logical request,
must be owned and known. Missing padding never becomes implicit zeroes.
Hardware-updated MADR, BCR and CHCR readbacks become unknown after copying,
until a source write supplies their next value; final hardware values are not
invented.

The current supported domain requires ordinary DMA direction and control
settings, nonzero 64-byte block counts, a word-aligned source address in a
proven main-memory mapping, a fresh transfer-address write, and a transfer
that stays inside sample storage. Reverse transfers require known sample
bytes and writable CPU destinations. Zero block counts, wrapping transfers,
unaligned DMA semantics, request replacement, hardware cadence, and competing
sample writers are explicit unsupported domains. They are not silently
clamped or repaired. The original CPU still computes its literal register
values and retains its completed prefix.

## Programmed I/O and startup writes

The source `7D334` writes halfwords to the FIFO and changes the transfer mode
to manual write. The backend retains those halfwords, requires a fresh address,
type 4 and a matching positive generation, and records the reached request.
`servicePendingPio` copies precisely the queued bytes into sample storage,
without DMA rounding or an event. The initial supported domain is one chunk of
1–32 halfwords within storage; additional chunks without a fresh address,
address wrapping and competing writers refuse. The startup's 16 bytes at sample
offset `1000` are covered. Original odd-count extra halfword reads remain in the
CPU owner. Service cannot repeat a completed copy.

PIO transitions invalidate prior status observations. An unknown canonical
status halfword read can use an explicit external platform callback; its result
is not cached. Other unknown registers, unknown RAM and unsupported widths do
not acquire a fallback. No automatic service, busy-bit timeline or completion
readback is fabricated. Direct `readDevice` continues to report unknown status
until actual incoming provenance establishes it.

Startup volume, voice and configuration writes retain their literal values
for inspection through `writtenConfiguration`. This separate query does not
claim the corresponding hardware readback or voice synthesis. Key-command
semantics and readbacks still require the external platform. See
[the initialization workflow](spu_initialize_workflow.md) for the recovered
startup sequence and the explicit integration-test conditions.

## Interrupt and event ownership

After a DMA copy, the request waits for its interrupt owner. The caller brackets
execution of recovered `7D668` with `beginIsr` and `finishIsr`; the backend
does not claim the original interrupt controller dispatched it. A refused
interrupt retains copied bytes and the executed CPU prefix, without claiming
safe replay. A completed copy cannot be serviced a second time.

The event registry allocates actual native opaque handles. It supports the
documented noninterrupt event mode, with explicit open, enable, disable,
close, deliver, and consuming test operations. No handle is ready by default.
The handle representation is native, not a claim to reproduce Sony BIOS
handle bits. The recovered ISR's custom callback replaces default delivery,
and unavailable callbacks refuse. WinMM playback state is unrelated.

An old pending event survives a new transfer start. Thus the original mapping
wait can return before the new copy if it consumes a stale event. This behavior
is preserved and tested rather than hidden by clearing the event on submission.

## Upload composition and limits

`spu_transfer_mapping.c` connects the existing heap/mapping bridge to the
recovered transfer and TestEvent owners. All components share the caller's
retained registry. The bridge accumulates access and journal budgets across
callbacks. A required final return that remains unknown refuses before mapping
branches on it. Other platform operations forward unchanged or refuse if
unimplemented. The bridge does not run a scheduler or fabricate completion.

The integration test runs actual SPU allocation, mapping, transfer register
programming, owned byte copying, recovered ISR completion, and event
consumption. Its scheduling and incoming state are explicitly prepared test
conditions. It checks rounded stereo copies, including padding overwritten
by a subsequent small allocation, the original right-length comparison after
copy, retained allocations on refusal, unknown padding, stale events, and
unload retaining samples and mapping fields. These results do not establish
natural startup, a full asset-loading session, voice synthesis, or a match.

Private checks also run 18 actual asset slices through program registration,
PATL relocation and this complete native upload chain, with only file bytes
mapped known. Sixteen complete with verified sample bytes and consumed events.
Two require 14 and 30 additional bytes beyond their owned file ranges after
DMA rounding and refuse before copying. The original allocation/loading
provenance of those extra bytes is still required; zero padding is not supplied.

Copying the backend gives independent sample, request, register and event
storage. The caller rebuilds temporary CPU/context bindings for the copied
owner and retains allocation-generation identity. Native ownership failures
do not roll back source effects or authorize resuming an interrupted original
function. Unfinished startup and unsupported device cases remain explicit
work for the full native port.
