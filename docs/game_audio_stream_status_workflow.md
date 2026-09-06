# GAMEONLY audio stream status recovery

`nba97_game_audio_stream_status` owns the full GAMEONLY inclusive boundary
`0x8008472C..0x8008480F`. Fresh Ghidra analysis identifies a 196-byte,
49-instruction function body with SHA-256
`6bb2afd423750ae33eb791180f4e0e1c9cb43c80163f7516db94a6b753ae11fd`.
The complete 228-byte, 57-word inclusive span has SHA-256
`8a972b73dca8932a878dccf456b7d65969bb2ab00675a5cd56d41fe27ad4a96c`.
These measurements describe different sets: four redundant `J 0x800847FC` and
NOP pairs at `0x80084760/64`, `0x80084788/8C`, `0x800847B8/BC`, and
`0x800847E8/EC` lie in the inclusive span but are unreachable and excluded
from the Ghidra body. The owner retains them as static source annotations and
does not claim that their PCs execute.

The leaf allocates an eight-byte frame, stores `s8`, and uses `s8` as its frame
base. It first reads flags at `0x800C43B0`. A clear mask `2` returns signed
`-14`. Otherwise it reads busy at `0x800C43B1`; any nonzero byte returns `4`.
When busy is zero, it reloads flags separately to test mask `1`. A clear bit
returns `1`; a set bit causes a third flags read and mask `4` returns `3` when
set or `1` when clear. The repeated reads remain separate so retained-memory
aliasing and live mutations preserve source behavior.

The model retains all 32 GPRs and one knownness bit per little-endian byte.
`LBU` and both `ANDI` instructions preserve their source upper-zero behavior.
Stack address arithmetic wraps as 32-bit MIPS arithmetic, mapped accesses keep
source order, and alignment, mapping, unknown branch, unknown address, budget,
and unknown `JR ra` prefixes report their exact source PC. The epilogue moves
the live frame base to `sp`, reloads `s8`, adds eight to `sp`, and consumes the
unchanged live `ra`.

`game_audio_stream_status_adapter` binds the actual recovered V stream-pump
event `0x80083F00 -> 0x8008472C` to this owner and forwards the complete live
GPR file and retained memory. V's other services remain explicit typed
fixtures. Under stable test memory, flags `5` make X return `-14`, so V takes
its early exit; flags `7` return `3` and V dispatches mode 5; flags `6` return
`1` and V dispatches mode 4; nonzero busy returns `4`, which is still a
nonnegative V gate result rather than a boolean failure.

The ownership search found only call-site references, the unresolved function
CSV row, and the older narrow `nba97_music_stream_status` projection. That
projection documents another source return table and accepts copied host bytes;
it does not own this address, execute the stack/global access sequence, retain
all GPRs, or model per-byte knownness. X therefore remains the sole complete
owner of `0x8008472C..0x8008480F`.

The focused asset-free tests cover all 256 flags, busy values 0, 1, and 255,
all return paths, repeated-read mutations through stack/global aliasing, exact
access journals, all 32 GPRs, partial knownness, stack wrapping, alignment and
mapping failures, every operation-budget prefix, and repeatability. The
integration test executes the production V owner through the production
adapter for flags 5, 6, and 7 plus the nonzero-busy case. No retail asset or
binary fixture is required.

Visual classification: **no direct visual effect**. This leaf reports retained
CPU audio state. It does not render, advance gameplay, or by itself prove
audible playback. Native capture and shared build registration are owned by
manager integration.

The manager's native input driver binds this leaf at five real stream-pump
calls. Raw flags 7 with busy 255 return 4, flags 6 with busy zero return 1,
and flags 7 with busy zero return 3. Raw flags 7 replace the earlier mode-5
gate fixture because flags 5 correctly return -14 through this leaf. The
parent pump still uses explicit lower stream-service fixtures. Receipts log
all four-to-six accesses, nested guest frames, and preserved return addresses.
Before/after diagnostic scanouts match; the separate frontend frame is User
Setup. No audible playback or advancing match is claimed.

Manager private differential: 12,288 cases compare full memory, all 32 GPRs and every budget prefix across all 256 flags, busy 0/1/255, and stack-to-flag aliases. All 49 body instructions execute; the eight excluded jump/delay instructions are statically checked only. All 237 asset-free CTests and native receipt checks pass.
