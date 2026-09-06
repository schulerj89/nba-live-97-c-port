# GAMELOAD main loader lifecycle recovery

`nba97_gameload_main` owns exactly GAMELOAD `0x801E136C..0x801E140F`,
164 bytes and 41 instructions. The evidence is the freshly exported listing
`gameload_801e136c_continue.txt` and the mapped GAMELOAD.BIN bytes with SHA-256
`a2d2a4b742c47b1c72d89e7c8b2ddbada0fee604cef947e11914515653e82398`.
The routine builds its 24-byte frame, calls eight GAMELOAD services, preserves
the bytes at `0x80015008` while GAMEONLY is loaded at `0x80015000`, and invokes
the late-loaded GAMEONLY entry through `JALR`.

The owner carries all 32 GPRs, HI, LO, and one knownness bit per byte. Guest
addresses remain numeric 32-bit addresses over caller-supplied retained
regions. The two frame stores, two global loads, and two returning-path frame
loads use exact little-endian byte and knownness behavior. SP addition wraps as
the original `ADDIU` does. Unknown effective addresses, misalignment, absent
regions, malformed knownness, operation limits, and refused services retain the
completed instruction, access, and callback prefix.

The natural parent adapter binds committed GAMELOAD entry site `0x801E14AC`
to this owner. It requires the exact event `(pc=0x801E14AC,
delay=0x801E14B0, entry=0x801E136C, operation=2081, invocation=1, site=2,
argc=0, program=GAMELOAD)` and `RA=0x801E14B4` with all four bytes known.
Every GPR word/mask and HI/LO word/mask is copied explicitly between the two
public machine types; the adapter uses no layout cast. A returned GAMELOAD main
is reported as returned so the committed parent executes its source `BREAK 1`.
A GAMEONLY transfer is reported as transferred and the parent exposes the
child's complete live machine.

The nine child boundaries are explicit and ordered:

| call PC | target | argc | role |
| --- | --- | ---: | --- |
| `0x801E1374` | `0x801E14B8` | 0 | constructors/startup |
| `0x801E137C` | `0x801E000C` | 0 | GP setup |
| `0x801E1384` | `0x801E059C` | 0 | hardware setup |
| `0x801E1394` | `0x801E0938` | 2 | registration pointer/value (`A0=0x8001000C`, `A1=707`) |
| `0x801E13B0` | `0x801E1344` | 3 | stage bytes using late-loaded S0 |
| `0x801E13C4` | `0x801E1300` | 2 | load `cdrom:GAMEONLY.BIN` at `0x80015000` |
| `0x801E13CC` | `0x801E1670` | 0 | interrupt callback shutdown |
| `0x801E13E0` | `0x801E1344` | 3 | restore bytes using callback-live S0 |
| `0x801E13F4` | late `LW 0x80015000` | 0 | typed GAMEONLY entry |

Every callback receives and returns the complete machine. A direct GAMELOAD
child must return; a reported transfer at any of the first eight sites is a
malformed composition and returns `NBA97_TEXT_ARGUMENT` with the callback's
machine and completed prefix preserved. The ninth child can return or transfer.
A transfer completes with the child's live machine and no fabricated epilogue.
On return, RA and S0 load through callback-live SP, SP increases by 24, and the
restored RA is checked only after the `JR` delay slot executes. The dynamic
target is latched before the `JALR` delay slot and is likewise checked after
that delay slot.

The focused direct suite covers all 41 PCs and all nine sites, exact delays,
targets, argument counts, call-entry machines and masks, ordered accesses,
every operation-budget prefix, refusal at every site, rejected direct
transfers, callback-live S0 and SP changes, partial globals, absent knownness,
malformed machines and regions, late target and RA faults, deterministic
journals, and distinct return and transfer outcomes. It passes 585 checks with
MSVC `/W4 /WX /sdl /RTC1 /Od /Z7 /MDd`.

The natural integration suite passes 534 semantic checks. Its fixture retains the
normal 2 MiB GAMELOAD region plus an explicit synthetic 24-byte frame region at
`0x807FFFE0..0x807FFFF7`. That separate fixture region maps the frame below the source-created
`SP=0x807FFFF8`, which comes from the raw stack-top word `0x00800000`; neither
the owner nor adapter clamps or rewrites that CPU value. This fixture does not
claim a production PS1 RAM-alias mapping. Tests run with and
without knownness planes, cover the returned-to-`BREAK 1` and transferred
paths, verify all 34 propagated machine words and masks, reject every malformed
parent field, retain every nested refusal prefix, and verify fresh binding
reuse and capture determinism.

`captureGameloadMain` is an asset-free diagnostic fixture. Its contract is a
synthetic standalone 2 MiB main-memory region and synthetic complete machine.
The startup service writes `0x80015098=1`; the two copy services perform bounded
4096-byte copies; the loader writes a synthetic GAMEONLY image with entry
`0x80020000`; the other direct services preserve the full CPU and RAM. The
fixture deliberately refuses the dynamic GAMEONLY call because no GAMEONLY
runtime is bound. Its receipt reports the input and stopped output machines,
numeric stop PC and target, result and completion flags, copy checksums, exact
access/PC/call counts, and deterministic fingerprints.

Fingerprints use FNV-1a-64 with seed `cbf29ce484222325`. Each integer is
serialized little-endian. A machine is 34 repetitions of `(u32 word, u8
known_mask)`, 170 bytes total. An access is `(u32 pc, u32 address, u32 value,
u64 operation, u8 width, u8 known_mask, u8 kind)`. A PC is one `u32`. A call is
`(u32 pc, u32 delay_pc, u32 target, u64 operation, u64 invocation, u8 site, u8
argc, u8 program)` followed by the 170-byte machine. No struct padding or native
`sizeof` participates.

The receipt also emits a bounded nine-element `call_sequence`. Each element
contains the exact PC, delay PC, target, operation, invocation, site, argument
count, program identity, and the full 170-byte-equivalent machine rendered as
numeric words and masks. The call fingerprint serializes the same event fields
followed by that complete machine. `call_overflow` is part of the receipt and
any overflow marks the fixture contract failed. The boundary field starts with
the actual `0x801E1374 -> 0x801E14B8` production startup service, lists the
remaining source children, and ends with the fixture's refused synthetic
GAMEONLY target `0x80020000`.

Gameplay shown: **BLOCKED**. The synthetic receipt proves this recovered
GAMELOAD routine reaches the refused GAMEONLY service after its exact source
prefix. It does not prove filesystem services, a production GAMEONLY image, a
tipoff, or an advancing match loop.

Final manager MSVC Debug validation passed 606 focused and 534 natural
integration checks. The independently decoded private raw comparison passed
3,000 cases covering all 41 PCs and nine call sites, all 34 CPU words/masks,
full 2 MiB RAM/knownness and exact PC/access/callback journals, all budgets,
refusals, live stack mutation and late dynamic targets. The tested semantic
core differs only by a neutral source comment from SHA-256
`59b5b5a3071857593b0db825ad4f09883c2a8bc5b9e61ac4ba1a7eb646709813`;
final C SHA-256 is
`5f8bf8fabd78e00759b4e22e2a9f39fc2ff83ef6983ab9fdd588505578bf0b3b`.
The native input run captured 130 frames in
`.local/verification/team_select/game-entry-20260906-152419-b703f510`.
Its independent verifier reconstructs the 36-PC refusal prefix, four accesses,
all nine child machines, input/output machines and fixture payload checksums.
Receipt: `frames/gameload_main_verified.json`.
Before/after pixel SHA-256 is
`42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`.
The inspected frame remains User Setup. First missing production child is
`0x801E1374 -> 0x801E14B8`; load services and the live GAMEONLY loop remain
unbound. No fixture output establishes tipoff.

All 417 asset-free CTest tests passed in the final Debug build (35.83 seconds).
Progress/recovery/instruction/roster freshness checks passed. No capture media,
private source data or other recovery assignments are part of this commit.
