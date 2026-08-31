# Original game audio startup

`recovered/audio_startup.c` implements four FEONLY routines as portable C:
`700B0` (62 source instructions), `7DEA8` (229), `73A68` (77), and `8E0E0`
(15). The 383 original instruction locations are checked independently against
the retained FEONLY bytes. GAMEONLY addresses are a separate namespace. The
production implementation uses native functions, not an instruction interpreter.

`700B0` runs sound initialization (`7E6EC`), initializes the SPU heap with 128
descriptors at `800FEE50`, applies common attributes twice, resets music state,
and conditionally registers `8007A6A8`. It ignores its incoming arguments and
the ordinary lower raw returns. A native refusal still stops at its reached
prefix; a terminal controller transfer ends the entire startup invocation.

The caller's common-attribute block has 40 bytes. The first pass writes mask
`2C3`, zero master/CD volumes and enabled CD mixing; the second pass uses the
same mask with volumes `3FFF`. Only the 16 bytes actually written by the source
become known. Unused fields stay unknown. Native local storage has no invented
source stack address: `PARAMETER_STORE` journal addresses are byte offsets, and
`stopped_local` identifies an offset when a local access stops. Standalone
`7DEA8` reads its actual parameter address through the retained RAM registry.

The common setter caches its mask once. Mask zero enables all attributes.
Master-volume sweep modes read the actual live jump-table words. Negative or
out-of-range signed modes select direct volume; supported sweep modes clamp
the signed input to 0 through 127. The native function supports the original
eight same-side case-entry targets, including default, and altered permutations
of those targets. Other encoded targets explicitly refuse after the table read;
they are not replaced with a guessed mode or an encoded-address function cast.

Every source pointer and parameter access retains its original ordering.
CD/external volume writes read the device base before the parameter. Control
updates read the enable flag, cache the base, read control, and write through
that cached base even if the callback changes the pointer. Only selected bits
are changed. The final raw return is zero if mask bit `2000` is absent, or the
cached device base if that final attribute executes, including the all-fields
case. It is not a backend success code.

`73A68` performs 30 ordered stores to selected music fields. It does not clear
the whole region or make untouched gaps known. Its raw return remains
`FFFFFFFF`. `8E0E0` scans eight callback slots and stores into the first zero
slot. Registration does not invoke a callback or establish a timer cadence.

Original bugs and unusual behavior are preserved and commented in the code:

- A full callback table silently returns zero, just like a successful insertion.
- Startup sets its registration guard before attempting insertion. If the
  table is full, the guard stays set and later startup calls skip registration,
  even if a slot subsequently becomes free.
- A null callback writes zero and leaves the slot available.
- Repeated startup still performs the sound/heap/volume/music initialization;
  the guard protects only callback registration.
- Lower ordinary raw returns remain ignored. Native ownership/refusal results
  are distinct from those original return bits.

`audio_startup_backend.cpp` composes the existing sound initialization and SPU
heap owners using the same retained RAM, controller, event registry and sample
storage. Common device accesses use the existing sound initialization route,
including CPU allocation generations for FIFO/DMA request producers and the
existing device/RAM ambiguity checks. It adds no device cache or source-memory
defaults. A copied retained state requires rebinding its borrowed pointers.

The outer CPU journal/budget, immediate sound initialization and heap budgets,
and nested controller/event/PIO budgets remain distinct. Lower prefixes and
completed transfers are recorded without rollback or automatic replay. Callback
reentry is rejected. Active source stack/code aliases and mapped RAM overlapping
owner/journal/progress metadata are outside the API contract.

Source comparison covers the original branches, sweep modes and clamp edges,
live table and RAM mutations, callback-table fullness, and stopped prefixes.
Native tests exercise the full startup chain, actual heap descriptor writes,
explicit PIO service, retained reset gaps, generation separation, cloning,
terminal transfers, and budget/journal boundaries. Hardware observations and
PIO service points in those tests are explicit test conditions, not proof of
physical PS1 timing or natural host boot.

The existing `music_voice.c` already implements `7A6A8` and its voice service.
Connecting the original interrupt/rate producer to that same retained clock
and 24-voice state remains separate work. Registration alone does not justify
calling the timer, inventing cadence, synthesizing audio, or claiming complete
frontend/match playback. Natural resource allocation and rounded-tail ownership
also remain open at their previously documented boundaries.
