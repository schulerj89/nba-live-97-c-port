# GAMEONLY camera frame transform recovery

## Boundary and evidence

This owner translates only GAMEONLY `0x80051098..0x80051293`, 508 bytes and 127 instructions. The source is the fresh Ghidra listing `.local/evidence/tipoff-recovery/game_80051098.txt`; its instruction SHA-256 is `e6df654e5e06920a44bd20d440af798af4c58e44f4c82c0be72a39c5b8bc54d2`. Known callers are `0x800490B4` and `0x800360C8`.

## Source behavior

The owner reads the camera gate before allocating its 0x30-byte frame. A zero gate invokes the controller child. It then reads angles, position, and offsets in exact source order; masks each retained angle to 12 bits; copies the position halves; and writes wrapped angle sums to its live stack before requesting matrix construction.

After matrix construction, the routine scales the first three signed matrix halves by 16/10 through the original three `MULT`, `MFHI`, arithmetic-shift, and subtract sequences. It preserves multiplication HI/LO and partial-byte knownness, including exact invariant product bytes obtained by enumerating the 65,536 possible signed source halfwords. The translation words are cleared in their original interleaving. Rotation and translation installation are separate typed calls. The reference transform receives fixed A0/A1 addresses and `live sp + 0x18` in its delay slot. Final translation adds signed copied position halves to full transform words with 32-bit wrapping.

Callbacks may mutate every GPR, HI/LO, mapped memory, S0, S1, and SP. The next source instruction consumes that state. The epilogue reloads RA, S1, and S0 through live SP and applies the wrapped frame increment before `JR` validation.

## Typed call order

| PC | Target | Arguments | Delay slot |
|---|---|---:|---|
| `0x800510B4` | `0x8004EA88` | 0 | NOP |
| `0x80051168` | `0x80056080` | 2 | `SH a2, 0x14(sp)` |
| `0x80051204` | `0x80055F18` | 1 | `a0 = live s0` |
| `0x8005120C` | `0x80055F44` | 1 | `a0 = live s0` |
| `0x80051228` | `0x80056650` | 3 | `a2 = live sp + 0x18` |

These children remain typed full-machine dependencies. Existing renderer and GTE helpers do not expose a compatible standalone machine boundary, so no child algorithm is copied or assigned fabricated register effects.

## Natural caller

`nba97_game_camera_frame_transform_from_match_frame` composes the existing `nba97_game_match_frame` call at `0x800490B4`. That state-level callback carries no machine payload. The binding therefore requires an explicit independent full machine and mapped memory, validates a fully known RA of `0x800490BC`, and does not infer an ABI from the caller's two unused scalar argument slots. Other match-frame calls pass to a configured fallback.

The natural fixture runs the actual match-frame owner through `0x800490B4`, executes this owner and its typed children, then stops at the next match-frame operation-budget boundary. Invalid call metadata or machine knownness leaves the configured entry machine untouched and records `NBA97_TEXT_ARGUMENT`.

## Validation

Asset-free runtime fixtures cover both flag paths; all five callsites, arguments, return addresses, and delay slots; angle mask and addition boundaries; signed matrix extremes; all four halfword knownness masks with independently enumerated product-byte invariance; all three MULT/MFHI prefixes and final HI/LO; signed position and transform wrapping; callback mutation and live stack relocation; stack/global aliases; unknown decisions and pointers; refusal and malformed-child prefixes; metadata, mapping, alignment, unknown-store, wrapping-SP, and JR failures; LUI/access order; and every cutoff in the 47-operation normal path. Tests use always-active checks under `NDEBUG`.

## Classification

Gameplay shown: NO - no direct visual effect. This is CPU camera-matrix preparation. Until an actual renderer consumes the recovered boundary, the evidence proves machine and memory state rather than advancing rendered gameplay.

Manager validation passed 99 focused executions, 5 natural-caller executions, and all 277 asset-free CTests. Private original-instruction comparison passed 2,050 cases across all 127 instruction sites, comparing full 2 MB memory, 32 GPRs plus HI/LO, callback entry states, aliases, stack relocation, and operation limits. An independent completion-set comparison additionally passed 84 partial-known cases across all three multiplication sites.

The native self-driving receipt uses the actual frame owner and production adapter with independent machine and typed camera/GTE fixtures. It records 47 operations, 22 reads, 21 stores, four callbacks, and translation changing from `[0,0,0]` to `[104,205,306]`. Before/after CPU frame SHA-256 is identical: `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`. The frontend remains User Setup; this does not establish a live camera or tip-off.
