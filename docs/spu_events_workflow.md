# Original audio event setup and shutdown

`spu_events.c` recovers 178 FEONLY instructions across event setup `7E4C4`,
shutdown `7E81C`, callback registration `7E548`, indirect dispatch `7F630`,
DMA callback installation `7FDB8`, interrupt-mask exchange `7F6EC`, and six
BIOS critical/event thunks. It operates on the caller's retained memory
registry. Device and BIOS effects require a native implementation; missing
operations do not receive successful placeholder results.

## Startup and original quirks

Setup reads `C7A80`. A nonzero value returns that value without other effects.
Otherwise it publishes the guard before entering the critical section,
clearing `C7620`, registering callback `7D668`, opening the noninterrupt event
for class `F0000009` and spec `20`, storing its handle at `C7678`, enabling
that handle, and exiting the critical section.

The original ignores registration and enable return values. It stores and
uses the raw OpenEvent result without checking success, including zero or
negative bits. A native unknown result is different: ownership cannot invent
the handle bits, so execution refuses before the store. Earlier effects stay
intact. In particular, a failed setup leaves its published guard intact, and
a fresh invocation can skip the incomplete setup. This original behavior is
preserved and commented, not repaired with rollback or an automatic retry.

The registration dispatcher reads live `C7DC4`, then the callback pointer at
that structure's offset 4. Target `7FDB8` executes the recovered owner directly.
Other encoded targets require the corresponding actual platform callback.
No raw address is cast to a host function pointer. The earlier producer of
the dispatch table belongs to separate interrupt-controller initialization;
this event owner does not fabricate it.

## Callback registration and interrupt masking

`7FDB8` reads the prior callback before its equality and initialization tests.
It returns that prior callback even when registration is skipped. The original
does not bound the channel index: callback-slot arithmetic and variable shifts
retain their 32-bit wrapping behavior, while native memory ownership refuses
unowned accesses.

When a change is accepted, `7F6EC` reads a halfword through live `C7E30`, writes
the supplied new mask, and returns the old halfword. The installer uses zero
to mask interrupts during the update, saves the prior mask, and restores it
through a fresh pointer read. This exchange is separate from BIOS critical
section operations.

The installer caches `C7E5C` before writing the callback slot, then updates
the reached DMA interrupt-control register. That order matters when a channel
slot aliases the pointer itself. A callback slot can also alias `C7E30`,
redirecting the later mask restoration. These source aliases are retained.

## Shutdown and return knownness

Shutdown enters only when the guard equals 1; other guard values return the
literal value 1. The active path clears the guard, enters the critical section,
clears the two software callback globals, unregisters the DMA callback,
closes the current event, rereads its handle, disables that handle, and exits.

Close occurs before Disable. Both raw returns are ignored, and the stored
handle is not cleared. The port preserves this sequence, including a handle
changed by the close operation before the second read. It does not insert
cleanup that the original never performed.

Completed external calls retain their raw return knownness. The final
ExitCritical result can remain explicitly unknown; setup and shutdown do not
invent a conventional success code. Other unused returns may also remain
unknown. A malformed knownness marker is reported after recording that the
callback already executed. A required device read must supply known bits.

## Verification and remaining integration

Public tests check source guards, failure bits, callback/handle aliases,
unknown results, exact store ordering and bounded prefixes. Their scripted
device and BIOS results are explicit test conditions, not hardware startup.
Independent comparisons execute the original FE instructions and compare
ordered effects, retained bytes and annotations, return bits, and refusal
locations in both native build configurations.

`SpuEventBackend` connects these operations to the same owned sample/event
registry used for uploads. Setup allocates the native handle through the
original call path; no prepared successful handle is inserted. Closed-handle
records preserve ownership for the original following Disable operation. That
operation completes without reviving the event, while leaving its unproved
ROM return explicitly unknown. Unowned handles still refuse.

Native critical-section state has no nesting counter. Enter disables and
returns the prior enable bit only when known. Exit enables and leaves its raw
return unknown. Neither operation is substituted for the separate I_MASK
halfword exchange. The adapter owns only the ordinary DMA registration domain
with no pending, force, or reserved flags; general interrupt acknowledgement
and timing remain unsupported. Delegated device accesses keep the external
owner consistently rather than mixing an external read with a local write.

The mapping integration test now performs original event setup before actual
allocation, transfer, sample copying and interrupt completion. It then tests
original shutdown and reinitialization, including the retained closed handle
and a newly allocated replacement. Earlier controller and sound-device state
remain explicit incoming test conditions; this does not claim full startup.

Access and journal limits retain the completed prefix. They do not authorize
resuming an interrupted original function or replaying external effects.
Source code/active stack aliases, concurrent registry mutation, and overlap
between mapped data and journal/context storage are excluded.

This owner closes the original SPU event lifecycle and registration CPU code.
Full controller initialization, interrupt dispatch/cadence, sound-device
initialization, audio synthesis, and natural game startup remain separate
requirements of the native port.
