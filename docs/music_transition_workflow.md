# Frontend resource music transition

`music_transition.c` projects the music portion of FE31A88 into two explicit
boundaries. It does not replace the intervening frontend resource loader or
its dispatch callbacks. The native playback files remain frozen from their
separate tested checkpoint; this new owner has not silently been wired into
them.

The source is private FEONLY.BIN SHA256
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
The first boundary covers31ADC through31BF8, after the preceding31A88 callee
effects and before2FB00. The second covers31F10 through31F20, after31F48
graphics/resource work returns. This is before outer3F7C8 dispatches the
selector initializer, including5A538 for View Player. Addresses omit the
80000000 runtime prefix.

Recovered decisions

The beginning clears selection-block F9720. It then tests whether the new
resource is1F with ED270 zero, is24, or whether previous ED2AC is nonzero.
When any condition holds, it calls FINISHED. If the stream is not finished,
it calls `FADE(voice,50,-1)` and writes routing phase4 after that call. In either
case it writes selection-block1, including when already finished.

For new resource24 or previous ED2AC nonzero, the source calls7760C on the
opaque F84C8 resource handle, including a zero handle, then clears F84C8.
On entry to24 it saves the current unsigned21D7C music option byte to the
32-bit F8F64 field. When adjacent option21D7D is nonzero, it replaces the music
byte with `min(current,(adjacent+1)>>1)`. When adjacent is zero, it leaves music
unchanged. On exit from previous ED2AC it restores the low byte of F8F64.
Finally ED2AC becomes exactly `resource==24`.

Resource24 is the identified frontend View Player state. This does not prove
a gameplay Pause caller. In particular, this transition is not source phase10
and is not equivalent to forcing the pause filename or calling the native
player's `requestSourceStop()`.

The end boundary clears selection-block for every resource except24. For24
it preserves whatever value the intervening resource dispatch left behind.
It does not force the value to1 again, nor does it automatically clear it.

Source quirks remain intentional:

* Zero adjacent volume skips music reduction entirely.
* Repeated entry to24 overwrites saved volume with the already reduced value.
* Exit restores the saved low byte even if the current option was edited.
* Already-finished streams still enter selection-block1 on qualifying entry.
* Resource release is called even with handle0.
* FINISHED/FADE can mutate live ED2AC before its second test. RELEASE can mutate
  the volume/saved state before the next reads; the callback's handle mutation
  is overwritten by the following source clear. These are not snapshotted.

Integration contract

Call `nba97_music_transition_begin` once at the actual source resource-entry
boundary, with the existing `Nba97MusicRouting`, mutable `Nba97MusicInputs`,
and persistent `Nba97MusicTransition` projection. Do not call it every update
or infer a new transition simply because the current page still renders.
The transition struct retains the saved volume, opaque released resource,
ED270 guard and adjacent option byte. Its state/input/callback arguments are
required; refusal returns0 before mutation.

The callback contract has three effects, in source order:

| Effect | Native integration boundary |
|---|---|
| FINISHED | Return the recovered music completion flag, not `isPlaying()` |
| FADE | Call the recovered voice fade for the supplied voice,50,-1; the owner writes phase4 afterward |
| RELEASE | Release the native resource corresponding to F84C8, preserving the explicit zero call; it is not automatically a music output reset |

Persist the mutated raw music option across subsequent routing updates. Do
not overwrite it immediately from an unrelated saved-settings snapshot.
Equally, do **not** call `setRecoveredVolume` merely because this owner changed
the option byte: the original transition performs no91748 GAIN call here.
Doing so would cancel the fade that it just requested. The routing owner's
next start reads the resulting raw byte and applies its normal recovered gain.
How transient source options map onto durable native settings is a caller
integration responsibility, not an instruction to persist temporary reduction.

Run2FB00 and the corresponding31A88 resource/graphics work through31F48,
allowing its documented effects on shared inputs, then call
`nba97_music_transition_end`. Outer3F7C8 selector initialization follows END;
5A538's PORT/COOL initialization is not an intervening begin/end operation.
See the [complete cleanup contract](frontend_resource_cleanup_workflow.md).
The two functions deliberately leave room for that intervening owner; merging
them into an unconditional UI event handler would conceal missing effects.

The exact other selection-block clear found in this audit is30EFC, inside
30E78. It executes only after the checksum comparison at30EE4 succeeds in a
resource-load callback. The checked native portrait loader now maps that
accepted callback to the same persistent music inputs before publication;
see the [photo workflow](player_photo_loader_workflow.md). Keep24 blocked
when no proven effect clears it. Do not substitute view-render completion or
automatically start ZTPAUSE.
Ordinary routing still reaches all five tracks through the original slot table.

F84C8's allocation/use includes3122C and its asynchronous loaded-resource
path; this projection only supplies its release boundary. The full resource
manager, preceding804E8/51534 effects, middle31A88 work and31F48 dispatch have
not been ported by this file. Native mutable adapter entry points are the
next integration work, after the frozen playback checkpoint review.

Proof

The standalone MSVC `/O2 /W4 /WX` C build and Release-safe public tests pass.
The private oracle compares6000 cases against actual instructions from both
source spans. It matches11360 callback events and complete projected state
snapshots across273469 instructions, covering all77 instructions in the
selected source spans. Cases exercise resource1F/24/ordinary/invalid values,
guard and prior-pause values, all byte values across the volume inputs,
finished versus active streams, low-byte restoration and callback mutations.
The end tests inject explicit intervening dispatch state and verify that24
preserves it while other resources clear it.

Private source bytes, disassembly, DLL hash, public-file hashes, build logs and
receipts are in `.local/verification/native_completion/music_transition/`.
This is an instruction/state/callback proof of the selected music boundaries,
not a claim of whole31A88 emulation, original resource I/O, gameplay Pause,
native View Player playback integration or SPU waveform equivalence.
