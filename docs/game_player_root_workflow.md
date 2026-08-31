# Original actor root matrices

`game_player_root.c` owns complete GAMEONLY `5200C` and its required leaves:
`56080`, `51F18`, `562CC`, `55F18`, `55F44`, `56650`, `51F04`, and `56624`.
The nine routines contain453 original instructions. This closes the immediate
producer before `52914` calls the already-owned `55368` part/hand routine.
It does not own the full frame caller, body-vertex projection, or rasterization.

The new C++ `GamePlayerRootGeometry` reuses the existing native
`GamePlayerGeometry` MVMVA implementation and `GameCourtGeometry` RTPS
implementation. It does not introduce another arithmetic implementation,
production CPU decoder, emulator dependency, or preview camera. Its `vector`
member is the retained state subset used by55368; screen/depth FIFOs, MAC0,
IR0, and projection controls remain explicit additional state.

## Input and output contract

Inputs use the existing body allocation references, cells, bounds and byte
knownness. `context_f0ed4` and `index_1029b0` identify live global **slots**.
The remaining references identify actual fixed resource/table bases. They
must be bound to those source objects, not invented aliasing or heap addresses.
The caller supplies current context `+4/+C/+8` X/height/Z, context halfword
`+16` yaw, the secondary pose reference at `+BC0`, the original template at
`26384`, original packed trig at `B3254`, actual camera matrix `F9FD8`, and
the actual per-actor scale table at `105F48`. Source index shifts and additions
wrap32 bits before native bounds checks.

For each reached physical index `p`, the routine produces:

| Object | Actual output |
| --- | --- |
| `FB480 + p*32` | Nine scaled Euler halfwords and three world-position words; upper matrix padding stays incoming |
| `103FD8 + p*32` | Five camera-composed rotation words, then three transformed position words |
| `10B2B8 + p*32 + 14` | Three camera-transformed words after negating the temporary Y halfword; rotation stays untouched |
| `102F8C + p*8` | Signed halfword X, literal zero Y, signed halfword Z; final padding stays incoming |
| `FEA94 + p*4` | Actual RTPS SXY2 packed center |
| `106038 + p*4` | Actual RTPS SZ3 shifted right two |

The ordinary successful path has37 retained stores. The source's stack writes
are private native scratch, with no fabricated source stack address. The
eight-byte26384 template is copied opaquely in its actual aligned-word domain.
Yaw replaces its second halfword before Euler construction. The fourth
halfword is discarded by the vector operation. Those two unused halves may
remain unknown; no zero-fill is required.

`GamePlayerRootGeometry` initially knows no camera controls. Provide actual
OFX/OFY, H, DQA, and DQB separately with knownness. Its vector rotation and
translation are loaded from the supplied camera bytes at the original points.
Projection controls are not required by earlier MVMVA operations. If H is
missing, for example, RTPS refuses at `56630` after the earlier35 CPU stores;
it does not fabricate a center or roll back those writes. The adapter carries
the same retained rotation, translation, V0, IR, MAC and FLAG through all math
operations. MVMVA leaves screen/depth FIFOs, MAC0 and IR0 untouched; RTPS
updates those fields exactly. Other device fields such as LZCR and OTZ are
outside this touched subset and must remain untouched in a shared owner.

## Preserved original behavior

* The `56080` Euler layout differs from the inline local matrix in55368.
  It negates selected **low32 products before arithmetic shift12**. Negating
  the rounded result changes negative rounding; the native code and a directed
  regression preserve the original order. Trig indices use signed-angle
  magnitude masked to12 bits. Values are not normalized or clamped.
* World height follows `520C4..520EC`: signed secondary root halfword shifted
  right four, multiplied by the raw scale with low32 wrap, shifted right16,
  then context height and literal36 added with wrap. `51F18` likewise scales
  nine signed halfwords using low32 products, shift16, and halfword truncation.
* `51F18` reads all nine halves before its first write. `562CC` reads matrix
  columns and runs actual MVMVA operations, then stores words in offset order
  `0,C,4,8,10`. Its final SWC2 IR3 writes the full sign-extended word, including
  padding. Matrix/source/destination aliases retain these reads and writes.
* `52120` rereads the scale after the ground-vector writes. The physical index
  is reread at the original later call boundaries and again after the screen
  store. A screen/index alias can change the last depth destination. There is
  no cached-index shortcut or repaired index range.
* `51F04` negates only the temporary Y halfword, including signed16 wrap. The
  two translations are not synthesized from a mirrored native model.
* `56650` stores MAC1/2/3, then reads FLAG and writes it to private stack.
  `56624` stores SXY2, writes IR0 to private stack, reads FLAG and SZ3, writes
  FLAG to private stack, and returns SZ3>>2. It does not substitute IR depth,
  LZCR, a floating-point camera, or a projected point callback.

Knownness and bounds are native guards, not repaired game behavior. Every
reached span validates all canonical metadata, including write-only bytes.
Masked halfword consumers validate the full original aligned LW/LWC2 span
but require only the bits actually consumed. A later malformed byte cannot
hide behind an earlier unknown byte. Scalar use or partial overwrite of a
relocated reference refuses when actual original address bits are required.
The native source-stack region is explicitly disjoint from retained input
allocations. No allocator, camera controller, or resource loader is faked.

Both CPU stores and geometry effects remain applied on refusal. This entry
is neither atomic nor resumable. Clone memory, reference cells, and the entire
geometry state if publication requires atomicity. Math callbacks may modify
geometry state only; they may not secretly mutate retained memory. A caller
must not advance to55368 after a partial root run as if5200C had returned.

## Verification and integration

Private evidence is in `.local/verification/native_completion/player_root/`.
A fresh read-only Ghidra export contains all nine routines; all453 instruction
words were checked against original GAME image SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
The5200C span itself hashes to
`c25214ea9688b9374efbaf59632b7557387a09bd23dadb528f32c97517a97285`.
The proof runs original CPU instructions without owner/callee hooks. Only
COP2 math uses the pre-existing independent private DuckStation reference;
that reference is never linked into production. There is no live-device claim.

Strict MSVC Debug/Release and GCC11.4 builds pass780 public checks, including
all37 journal cutoffs, asymmetric Euler rounding, live scale/index aliases,
wrapped indices, delayed camera/control requirements, and unknown padding.
The rotation-loader refusal test also identifies its last CTC2 at55F40, the
actual return delay slot, rather than attributing it to the preceding JR.
Each MSVC configuration also passes259 original CPU comparisons covering all
453 PCs,55 refusal prefixes and8,478 stores;60,000 independent arithmetic
comparisons retaining all64 geometry words; and32 partial-knownness probes.

An additional80 cases compose complete5200C directly into frozen55368 using
all five players from actual ordinary `ZDOMVATL`, `ZDOMVBOS`, `ZDOMWATL`, and
`ZDOMWBOS` pivots. Actual640D8-normalized ZMOCAP clips77/78/79/0 and blends
are sampled by original530FC, and40 native pose values are compared first.
All26,400 combined stores and final geometry state match original execution.
Camera, scale and context placement are explicit fixture inputs; this evidence
does not establish a natural initial frame or a recovered camera controller.

The new C/C++ files and asset-free test are registered in CMake using the existing player
geometry, court geometry, and `zdomf_projection`/`zdomf_transform` dependencies.
Use `GamePlayerRootGeometry::callback`, then carry its `vector` state and the
same retained body buffers into55368. Preserve the extra FIFOs/controls when
sharing the state with court rendering. Both the core tests and Windows
application compile this slice; the live match-entry boundary is unchanged.

The remaining immediate rendering work is the actual52914 caller's context/
global setup and525AC body-vertex/packet path. Camera `F9FD8` and scale `105F48`
still require real producer provenance when entering from a natural match.
This routine consumes them faithfully but does not replace those producers.
