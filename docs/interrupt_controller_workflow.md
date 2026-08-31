# Original interrupt-controller startup and dispatch

`interrupt_controller.c` recovers 556 FEONLY instructions for controller
initialization, shutdown, callback registration, interrupt dispatch, DMA
dispatch, VBlank initialization/callbacks, word clearing, and the reached
BIOS thunks. Device operations and BIOS effects require actual native owners.
The CPU implementation does not contain an emulator or a successful missing
callback substitute.

## Startup and retained context

`7F708` skips when the halfword at `C7DC8` is nonzero. Otherwise it enters a
critical section, clears 25 words beginning at that address, and captures a
context at `C7DFC`. A zero capture result continues initialization. A nonzero
result enters the actual interrupt handler `7F7C8`.

The ordinary path publishes the ready halfword, initializes VBlank and DMA
callbacks, and stores their returned registration functions through live
`C7DC4` at offsets `14` and `4`. General interrupt registration dispatches
through offset `8`, whose original static target is `7F9BC`. No prepared
successful DMA callback pointer is inserted by this CPU owner.

The next BIOS call, `A0/72`, removes the ISO-9660 CD-ROM driver. Its identity
must not be confused with interrupt initialization. The source then writes
the saved stack pointer at `C7E00`, hooks the captured context, and exits the
critical section. Saved context, BIOS driver/policy state and interrupt
hardware remain separate ownership responsibilities.

The BIOS capture result is required branch input. An unavailable result
refuses after the completed capture operation and earlier clear stores.
The original publishes readiness before several subsequent operations and
does not check their raw return values. Those behaviors remain intact; a
fresh call may skip an incomplete initialization. Native failures do not
roll back or automatically resume that prefix.

## Interrupt control flow

The top-level handler sets its active halfword, intersects the cached mask,
live I_STAT and live I_MASK, and scans the eleven source channels. For each
pending bit it writes the acknowledgment, reads status back, tests the
callback, rereads that callback and calls it. DMA dispatch performs its own
seven-channel scan and acknowledgment sequence. The dispatcher rereads
pending state after each pass; no synthetic timeout or channel clamp is
added to the source logic.

Callbacks naming the recovered DMA or VBlank handler run that code directly.
Other encoded addresses require a real native callback. Source addresses are
never cast to host function pointers. Device writes retain the original
values, including acknowledgment patterns; this CPU owner does not model
write-one/zero hardware effects by treating them as ordinary assignments.

`ReturnFromException` transfers control to the interrupted context. The
native callback can report this completed transfer with result `2`. It ends
the entire recovered invocation, including an enclosing startup invocation,
without running later initialization or manufacturing a return value.
A fixture that explicitly returns normally from this BIOS boundary can
still exercise the original code's fallthrough; that is not normal BIOS
exception behavior. Completed transfers from arbitrary callbacks propagate
in the same way. Transfers from an operation not permitted to transfer are
reported as invalid metadata after recording that the operation executed.

## Original edge behavior

General registration reads the old callback before equality and ready tests.
Channel arithmetic wraps without a source bounds check. Callback slots can
alias the cached mask or the interrupt-mask pointer, so stores and fresh
pointer reads retain their original order. Channels 4 through 6 additionally
call the original counter policy operation with the callback-null test.

The unhandled-interrupt counter is incremented before testing its previous
value using a signed comparison with `101`. Negative or wrapped values defer
the diagnostic. On the diagnostic path the handler rereads the device
values, prints, masks the currently pending bits, and clears status and the
counter in source order.

VBlank initialization clears its ready/callback words but leaves its counter
untouched. Its handler caches the callback before incrementing the counter,
then rereads the counter even when a callback will make that value unused.
The disabled VBlank setter returns its untouched incoming `v1`; the caller
must preserve its knownness instead of supplying a conventional zero.

Shutdown clears the original controller/context words and invokes the
VBlank/DMA shutdown helpers. It does not clear the installed function-table
entries and does not call `ResetEntryInt`. A native context owner must retain
the hook identity while recognizing that the saved context has been cleared;
it must not quietly detach or reconstruct a valid context.

## Verification boundaries

`InterruptControllerBackend` retains the native continuation and installed
hook. Capture marks unavailable saved registers unknown rather than retaining
the earlier cleared zero bytes as facts. Hook installation checks the live
source-produced PC/SP and records the context bytes and knownness. Subsequent
entry requires the same allocation generation and an intact context. It uses
the same critical-state and device owners as SPU event setup, without a
second register cache. ReturnFromException restores the interrupted native
critical state and ends the recovered invocation. Refused interrupt execution
retains its prefix and cannot be retried automatically.
The native context domain excludes nested IRQs and arbitrary callbacks that
switch threads, replace the saved context, or perform critical-section
system calls during interrupt execution. Those BIOS operations can destroy
the interrupted context; restoring an old native frame would not reproduce
that behavior. The ordinary controller/DMA/SPU completion path does not use
such operations.

ISO-driver availability and PAD policy can have explicitly imported native
ownership. Missing policy ownership requires the platform operation. Counter
clear policy, I_STAT, timer hardware and general DICR acknowledgment remain
required platform operations; this backend does not fabricate their effects.

The upload integration now starts from the original static table with its
DMA registration pointer still zero. Actual controller startup captures and
hooks the native context, installs the original general/DMA/VBlank callbacks,
and then runs original SPU event setup. After actual sample copying, an
explicit external device fixture signals the pending DMA source. Native
exception entry executes the recovered controller handler, DMA dispatcher,
registered SPU ISR and event delivery before the original poll consumes it.
There is no direct shortcut from copying bytes to calling the SPU ISR.

That fixture deliberately owns hardware acknowledgment and counter policy
for this composition. Earlier sound-device state and scheduling at the poll
remain explicit conditions. This is stronger startup/dispatch integration
evidence, but not proof of physical IRQ cadence, ROM internals, or a complete
natural host boot. The real-asset composition preserves refusal on rounded
sample bytes beyond the owned input file instead of inventing padding.

Public tests cover startup/shutdown, retained guards, failed-prefix behavior,
nonlocal exception return, registration aliases, combined IRQ/DMA/VBlank
dispatch, diagnostic thresholds, raw return knownness, and native limits.
Their BIOS/device results are explicit fixture conditions, not proof of
original ROM behavior or a complete host scheduler. Independent verification
executes the original FE instructions and compares ordered effects, retained
memory, raw return information, transfer disposition and refusal prefixes.

Access and journal limits bound work. A separate native limit of 64 nested
inline callbacks prevents malicious or accidental source callback cycles
from exhausting the host stack. This is an explicit native refusal at the
reached call site, not a repaired source callback or an original bound.
Source-code/active-stack aliases, concurrent metadata mutation and overlap
between mapped data and output/context storage are excluded.

This component is part of the complete native port. General hardware IRQ
timing, original BIOS policy edges, natural resource startup, voice synthesis,
and the full playable game still require their own implementations and
integration evidence.
