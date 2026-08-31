# Music voice service and completion boundary

`src/recovered/music_voice.c` recovers the source voice service, fade arithmetic,
gain arithmetic and final completion decision. It does not implement the CD
producer, SPU ring buffer, SPU waveform or a WinMM player. It can replace those
specific arithmetic/control boundaries in a host adapter; its callbacks keep
the remaining dependencies visible. See `music_routing_workflow.md` for the
five resources and source filename routing.

The source is private FEONLY.BIN, SHA256
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
All addresses below are FE runtime addresses without the80000000 prefix.
Original bytes, private headers and decoded audio remain under `.local`.

| Native boundary | Original owner and scope |
|---|---|
| `nba97_music_voice_timer` | Entire7A6A8..7A81C, calling recovered service |
| `nba97_music_voice_service` | Entire7A81C..7AD08, with hardware and optional callbacks |
| `nba97_music_voice_fade` | Resolved voice arithmetic inside7B2BC..7B3B4 |
| `nba97_music_voice_gain` | Resolved voice mutation inside91748..9180C |
| `nba97_music_voice_effective` |76334 effective gain, including optional lookup |
| `nba97_music_stream_status` | Entire6B6A0..6B784 return table |
| `nba97_music_hardware_status` |7BFA0 return table after selecting a physical voice bit |
| `nba97_music_voice_complete` |702B0 state1 ending arm70508..705C4, including916AC |

Fade/gain helpers deliberately do not claim source API validation or locking.
The adapter must check audio enabled first (-10), validate target/gain (-8),
lock, resolve the exact voice handle, run the helper, then unlock. Source916CC
accepts only active byte1 and an exactly matching handle at the low-five-bit
index. It does not restrict that index to the24 physical records; malformed
handles can read beyond the array. A safe host must reject an unrepresented
index as an explicit unsupported input, not claim that safety check is retail
behavior. Gain then calls effective gain and applies it before unlocking.

Timing and original quirks

The frontend setup288E8 stores120 to D9ADC, then288EC calls7844C(120).
70178 registers7A6A8 through8E0E0 in the eight slots at D96E8. The IRQ6 owner
78628 increments the source clock D9AB8 and invokes those registered callbacks.
Thus the audio timer receives the source120Hz callback, but its service is not
120Hz:7A6A8 computes unsigned `wrap(callbacks*100)/rate`. It invokes7A81C while
`services <= target`, including the original extra initial service. At120
callbacks from reset the native and original both have101 services.

7A81C increments a separate third counter, runs702B0, calls D9BC4 if nonzero,
and only then tests signed `third_counter % 3`. At multiples of three it calls
D9BB8/D9BBC/D9BC0 in order, then visits24 voices. Fade/envelope work therefore
averages100/3 updates per second. A fade argument is not a120Hz clock duration;
its exact stop time also depends on quantization, service phase and hardware
drain. The helper preserves all of the following source behavior:

- Nonpositive signed fade duration becomes1. Target-1 is negative gain followed
  by a stop request. It is not clamped to0.
- The ramp step is signed truncation of the wrapping32-bit delta multiplied
  by3, divided by duration. A sufficiently long duration can yield step0 and
  stall indefinitely. The helper does not repair this.
- Gain setting cancels the ramp but does not reset its target. A stop request
  during a ramp does not skip the remaining envelope/gain work in that service.
- Envelope countdown FFFFFFFF is decremented; it is not an infinite sentinel.
  Envelope index wraps as an unsigned byte, but count is read as a signed byte.
- A subsequent envelope stage divides duration by3, then uses **unsigned**
  division for its gain delta, including descending envelopes. Duration below3
  reaches the original divide-by-zero trap. The helper returns-1 with prior
  mutations retained; it must not be resumed after this fault.
- Timer rate0 likewise reaches a source trap. Counter multiplication wraps,
  and catch-up has no cap. Rate changes reset service/callback counters but do
  not reset the separate third counter. Locking defers timer calls by counting
  them; the original7AD48 drains that count when lock depth reaches0.
- Effective gain reads signed authored/master bytes and signed fixed-point
  high halves, wraps each product, divides by127^3, then optionally maps the
  signed resulting byte. No modern gain curve, saturation or absolute value
  has been substituted.

Actual music initialization

All five extracted CNK headers were run through original924B4 relocation and
9267C tone setup. Only allocation, lock/unlock, upload and hardware start were
explicit hooks. Every file produces authored/effective gain127, current ramp
127<<16, ramp step0, centered pan64, no gain/pan lookup maps, envelope step0,
envelope current127<<16, envelope count1 and envelope countdown FFFFFFFF. The
envelope words come from CNK offsets120/124. These are file-derived values,
not assumptions based on the separate6FDF8 special stream allocator.

After source91748 applies the frontend setting, ordinary music with master127
has effective gain equal to the setting's clamped `min(volume*15,127)`. Keep
the recovered arithmetic if other master gains or later states are used.
The host still owns exact left/right gain application through the source
71600/71EBC/71818 hardware boundary; this work does not prove SPU mixing or
the decoder's predictor rounding against original hardware.

Completion and buffer ownership

6FCF0 reads unsigned E45E7. Initializer7390C and cleanup73A68 set it to1;
72954 clears it at72F4C when starting playback buffers. It is not simply
`decoded_position == decoded_length`.

For the tracked streaming voice, stop92C34 resolves the handle and calls71A68.
71A68 sets C6D58 and returns without freeing the voice or setting E45E7. The
SPU interrupt7309C consumes that request at7313C and reaches734C4, which queues
key-off in C6D3C for the tracked voice and, for stereo, its pair.702B0 at7075C
submits that mask and writes logical channel state1. On a later service, its
state1 arm observes SDK7BFA0 status0, sets tracked voice E45E4 to-1 and E45E7
to1, clears the channel transient byte/state and clears voice.active through
916AC. The stale handle itself remains unchanged.

SDK7BFA0 status combines the software key-on mask and actual SPU ADSR level:
key-on/nonzero=1, key-on/zero=3, key-off/nonzero=2, key-off/zero=0. This distinction
is required before calling the recovered completion arm. The separate6B6A0
stream status returns only-14,1,3,4. Its zero result is unreachable; retain the
routing owner's unreachable zero branch instead of inventing an is-playing
Boolean that makes it reachable.

The native adapter should keep three separate facts: source stream/voice
state, whether a particular output buffer was returned by WinMM, and whether
its storage is safe to reuse. Keep a generation/token with each submitted
header so a late return from an old track cannot finish a new track. This is
host memory management, not a change to source selection or stop behavior.

WinMM reset marks pending buffers done and returns them even when their audio
was not played; a reset-generated completion must not be treated as natural
source completion. Unprepare only after the driver is finished, and free or
replace PCM/header storage only after successful unprepare. On reset or
unprepare failure retain that storage and report the host error. These are
platform requirements, independent of source phase4's120-clock retirement
deadline. [Microsoft waveOutReset](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutreset),
[Microsoft waveOutUnprepareHeader](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutunprepareheader).

A WinMM callback should post/record completion for the owning service thread;
do not reenter routing or call wave functions from it. Microsoft documents
wave callback restrictions and deadlock risk. This design also keeps the
source callback order deterministic. [Microsoft waveOutProc](https://learn.microsoft.com/en-us/previous-versions/dd743869(v=vs.85)).

Integration contract and remaining dependency

Use the original source120Hz clock producer to call `music_voice_timer` once
per elapsed source IRQ tick, not once per presentation frame. Bind the24
voice records and callback context on the owning thread. `HARDWARE_SERVICE`
must perform the hardware/channel lifecycle before fades, and `STOP` must
request source stopping rather than clear active immediately. `APPLY` receives
the physical index and signed effective-gain bits. Current music headers need
no map reads or later envelope stage reads during normal track lengths.

`music_voice_complete` closes the final state1 observation only. It does not
manufacture that state. The source stream producer remains an explicit next
dependency: SCEl reaches phase7 in6D158's stream machine; phase7 can consume a
queued continuation at stream+74 instead of ending the stream.72254/726C4 feed
the SPU ring and eventually mark C6D29; the SPU IRQ's73160 arm checks signed
read index E45EE against write index E45EC minus1 before queuing key-off.
Those producer/continuation conditions and precise hardware drain timing have
not been differentially implemented here. Finite WinMM output can supply an
explicit host approximation, but it must not be reported as a proven source
stream lifecycle or silently loop to bypass this boundary. ED2AC still does
not establish gameplay Pause reachability. XA playback remains outside scope.

Verification

Private `.local/verification/native_completion/music_voice/oracle.py` compiles
the current C source and public tests with standalone MSVC `/O2 /W4 /WX`.
It executes original FE instructions against the native code for1200 service,
600 timer,3000 fade,2000 effective-gain,512 resolved-gain,2000 completion,
288 hardware-status cases and all65536 stream-status byte pairs.18107 callback
events match with full projected state snapshots;4084835 original instructions
execute across721 distinct instruction addresses. Hardware/optional/stop/apply
boundaries are explicit hooks. Five real headers additionally pass original
relocation/tone initialization. The public tests retain regression cases for
the inclusive timer boundary, unsigned descending envelope, divide-by-zero
fault boundary, stale handle and asynchronous completion.

Receipts: private `report.json`, `source_mips.txt`, `cnk_voice_init.json`, and
`build` logs. No live playback, render/UI result, hardware waveform equality
or complete stream producer is claimed by this proof.
