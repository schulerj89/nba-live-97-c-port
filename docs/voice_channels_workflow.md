# Shared voice channel service

`voice_channels.c` implements the complete CPU owner702B0..70884, including
916AC's active-byte clear. It supplies the channel lifecycle between the
recovered announcer fade/stop request and handle retirement. It does not
replace the original voice allocator, SPU registers or stream transport.

Source is FEONLY.BIN SHA256
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
The373-instruction source extent SHA256 is
`4a84929a9dac24b4aec68826bbb0a3a731eec31c43b9a0bf9b8bfba4c1c041da`.

## Required shared ownership

`Nba97VoiceChannels` borrows the same `Nba97VoiceStopState` used by71A68,
the same24 F06B8 voice records used by `music_voice`/`voice_handles`, and a
pointer to the same E45E7 finished byte consumed by stream completion. Channel
state/transient project word+4 and byte+2 of F0D58+i*12. Channel kind/paired
voice remain in the borrowed stop state, not independent copied fields.

One service owns this state. Do not run independent music and announcer copies
of702B0 or the100-service clock. In the actual `music_voice` HARDWARE_SERVICE
callback, compose this channel owner with real backend operations. Existing
`voice_handles` borrows the same clock so outermost unlock can drain pending
timer callbacks through the same path.

## Complete source order

The owner first calls7AEE4 and sets C6D54 busy to1. If stream maintenance
C6D50 is nonzero while C6D2C and C6D2A are zero, it calls72954. If C6D30 is
then nonzero, it returns **without clearing busy**. This source quirk is
preserved; an unconditional cleanup assignment would change the owner.

It then processes physical voices0..23 in order:

| Entered channel state | Exact decision |
|---|---|
|4 |Skip the excluded C6D34 voice. Otherwise read7BFA0; a result other than1 changes state to1 and queues keyoff. Kind>=2 also changes the paired state and queues its bit. |
|2 |Read7BFA0; result1 changes state to4. Other results leave it unchanged. |
|1 |Read7BFA0; only result0 clears active. If this is the signed E45E4 tracked stream, first set that byte toFF and E45E7 to1. Then clear nonzero transient, set state0, and clear voice.active through916AC. The stored handle stays unchanged. |
|Other |No status call or repair. |

The entered state chooses the branch before its hardware callback. A callback
mutation does not cause the owner to restart branch selection. The actual
status word is retained in D9D0C, including negative/error bit patterns.

After the channel loop, enabled C6D40 auxiliary-on and then C6D44 auxiliary-off
masks call7E684 and clear their corresponding masks. Next nonzero C6D38 key-on
calls6F858(1,mask), scans its live bits and sets affected channels to2. Finally
nonzero C6D3C keyoff calls6F858(0,mask), scans its live bits and sets channels
to1. These key batches propagate paired state only for **kind==2**, unlike
the earlier state4/71A68 **kind>=2** predicate. Do not normalize this difference.
Each batch clears its mask after processing, even if a callback changed it.
Busy becomes0 only on the ordinary final path.

All callback state is live. A mask changed by6F858 is reread for each following
channel; a callback-cleared mask produces no state changes. Mask bits above23
are still submitted and finally cleared, but the CPU loop does not manufacture
additional channels for them. Paired indices24+ refuse when a paired state
access is reached, retaining the preceding current-channel mutation/mask.
The earlier71A68 bit-only operation continues to use MIPS low-five-bit shifts.

## Backend contract and limits

| Required callback | Boundary |
|---|---|
|SAMPLE_7AEE4 |Original platform sample call, whose returned word this owner does not subsequently consume |
|STREAM_72954 |Actual stream maintenance, not a fabricated no-op completion |
|STATUS_7BFA0 |Actual source hardware-status result for the supplied single voice mask |
|AUXILIARY_7E684 |Perform the requested on/off mask operation |
|KEY_6F858 |Perform the requested key-on/keyoff mask operation |

An adapter returns1 only after its operation was performed. STATUS supplies
the actual result bit pattern. A backend refusal returns IO_REFUSED with all
preceding effects retained; it cannot be treated as source completion. Missing
native arguments refuse before effects. The C return value is a native
completion result, not the unused702B0 return register.

The existing `music_voice` callback type cannot propagate such a refusal by
itself because the original hardware call has no checked return. Therefore a
host that composes this owner inside that callback must retain and surface
failure, and must not publish a successful source-service transaction after
refusal. If transactional staging is used, it must cover all shared voice,
clock, channel and pointed-to state. Platform operations already performed
are not rolled back merely by copying a C struct. Do not report a failed
backend as “finished” to release313C8 or3122C.

In particular, a stop request queues keyoff; submission of that keyoff changes
channel state but leaves the generation active. A later observed hardware0
in state1 clears active. A WinMM adapter can explicitly map its own current
generation's returned buffers into a native hardware state, but this is not
proof of exact SPU ADSR release or waveform timing. Stale generations must
never change a newly allocated voice's status or gain.

The source's natural-finish route also matters: a playing state4 channel whose
status ceases to be1 enters state1 and requests keyoff before a later zero
status retires active. Do not collapse that sequence into `WHDR_DONE` directly
clearing a source handle. The original stored handle remains stale after
retirement, and313C8/3122C retain their already-finished-handle quirks.

## Verification and next integration

Private receipts are in
`.local/verification/native_completion/voice_channels/`. Debug and Release
MSVC/W4/WX builds pass104 public checks each. Each configuration's original
instruction proof passes3000 cases,44900 callback events and6849103 executed
instructions, covering all373 direct702B0 instructions. Complete source/native
state and callback snapshots include status-side state changes, stream-side
changing flags and key-side mask replacement. Platform calls remain explicit
controlled effects, not real-device tests.

The public test additionally composes original313C8 cleanup with the full
handle/fade wrappers, pending timer/voice service and this channel owner.
A deterministic interrupt fixture supplies pending callbacks; its test backend
reports release after keyoff. The test verifies one keyoff, delayed active
clear and retained source handle. It is a CPU composition regression, not a
claim that one native status poll equals one original interrupt.

The next implementation dependency is a stable WinMM clip-generation owner
that retains headers/PCM/device storage until actual retirement succeeds.
The confirmed native defect and concrete injection/ownership design are in
`voice_handles_workflow.md`. The original program/sample allocator and
validated COOL allocation still need integration; these CPU owners do not
invent their successful return values. Existing audio/host files are unchanged
by this channel checkpoint, and no shared build, UI/device or Git operation
was performed.
