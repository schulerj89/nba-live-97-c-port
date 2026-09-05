# Gameplay motion resource: bounded resolver contract

2026-08-31. The native raw-file resolver and immutable byte owner are implemented
in `src/recovered/gameplay_mocap.*` and `src/gameplay_mocap.*`. This is a resource
boundary toward gameplay, not a native scene, animation sampler or possession.
Original files, source-oracle fixtures and RAM snapshots remain ignored under
`.local/verification/gameplay/`; native comparison receipts are under
`.local/verification/native_completion/gameplay_motion/run-20260831/`.

## Source ownership

| GAMEONLY function | Scope | Native support / full original instructions | Dependencies and evidence | Remaining uncertainty |
|---|---|---|---|---|
|800640D8 |Load and normalize two motion directories |Raw-file projection /132 |23 synthetic source cases,64 randomized source comparisons, actual file and saved demo comparisons |Loader retry behavior, caller integration and playback |
|80029BFC |Retry file loading until a nonzero pointer |Standalone retry owner /17 |Fresh Ghidra extraction, exact retry/stack unit cases, and native composition at loading-screen and FELOAD callers |Production 941C8 file/device/allocation implementation and lifetime integration |
|80090D60 |Return a loaded payload's requested heap size |Standalone heap query /9 |Fresh Ghidra extraction, exact null/wrap/stack tests, actual 90618 lookup composition, and native composition at FELOAD caller29B08 |Second callerA7200 and production 941C8 allocation ownership |
|800642E8 |Release resource and clear21490 |Not integrated /17 |90698; source audit only |Native caller teardown and stale directory users |

All denominators are complete functions. Source PC coverage is distinct from
native semantic ownership and earns no instruction credit. These three functions
are a bounded resource inventory, not a transitive gameplay completion score.

Normal initialization2DB90 calls640D8 before659F0 match initialization and period
setup. A separate conditional rebuild2E0EC releases the old resource and reloads
it before48D5C scene construction. Teardown clears21490 but leaves the two pointer
arrays unchanged. The rebuild even has an intervening66F88 use of old directories;
its complete lifetime must be audited before wiring that branch. A native owner
must not expose dangling pointers to imitate an incomplete caller reconstruction.

## File and header rules

The first two little-endian words give two file-relative directory offsets.
Each directory has84 entries. Zero entries are null; other entries are offsets
from the file base to12-byte headers. Traverse channel0 then channel1, ascending
slot order. Exact aliases within/across directories retain one header identity.
The first output array is8001EC98..8001EDE7; the second800170C8..80017217.

Header fields are flags(u16,+0), timing(u8,+3), count(u8,+7), and data reference
(u32,+8). Other bytes are untouched. When flag20 is clear, the data reference is
relative to its header, not the file. Original ADdu forms the pointer modulo32
bits and sets20. An in-file native offset needs a signed displacement and checked
64-bit addition. Backward references and shared stream data are valid.

Independently, if flag08 is set and10 clear, set10, halve timing, and double count,
subtracting1 when flag01 is set. Byte stores wrap modulo256. Preserve unrelated
flag bits. A repeated reference sees the mutated header and does not normalize
twice. A repeated call on retained normalized bytes is similarly idempotent.

The private disc file has200044 bytes,145 unique headers,158 nonnull references,
10 null entries and five backward data references. Twenty-five of84 channel pairs
have unequal normalized counts;10 have only one channel present. Downstream66F88
uses the larger count, with null contributing0. The frontend six-clip parser's
nonnull/equal-count/data-offset12 assumptions must not be copied here.

## Native implementation boundary

A focused portable C parser produces an immutable index: two84-entry optional
references, at most168 unique headers, file/header/data offsets, and both raw and
normalized flags/timing/count. A shared immutable C++ resource owns the input
bytes and index and publishes only after validation. Reuse its cached index;
decode fresh bytes for a new resource. No original address becomes a host pointer.

Native bounds checks must be labeled as guards, not retail rejection branches.
The parser validates directory/header extents and4-byte alignment and signed
in-file targets. Its output is a fixed-capacity168-header C struct, with capacity
guaranteed by the168 input entries; it does not accept a variable-capacity buffer.
It permits exact directory/header aliases, different headers sharing data,
nulls, unequal counts and unrelated flag bits. It rejects overlapping distinct
headers and headers overlapping control words/directories, which would
otherwise let original in-place writes alter later traversal. Do not infer a
payload extent or joint/frame stride from this resolver alone.

For the first immutable raw-file API, reject pre-relocated flag20 input. Its+8
word already holds an absolute PS1 pointer; it is a different encoding, not another
relative offset. A future RAM-import API would need an explicit encoding and
validated source base. Source normalization evidence includes those inputs even
though the disk API deliberately excludes them.

Keep src/zdomf_mocap.* and the verified Create Player pipeline unchanged. This
resource resolver requires no camera, player clock or animation selection.
Gameplay sampling, ZHOTS offsets, model layouts and event timing remain separate
dependencies in gameplay_first_path.md.

## Original demo observation

The existing reference emulator booted the local disc, reached the title and
entered its automatic demo without injected game keys. A newly derived GAMEONLY
signature found exactly one2MiB RAM backing after the FEONLY signature ceased to
match. Seven independent GAMEONLY code ranges match3,876 source bytes in each
of two saved dumps. This validates the overlay interpretation, not every loader
save/restore operation.

Both dumps have resource pointer80021490=80116C1C. Executing the original full
640D8 owner on the private disc file, with only its loader return supplied, yields
the exact200,044-byte live resource in each dump. All145 unique headers, all five
backward references and both84-entry output arrays match. The directories total
672 bytes and include10 null slots. All812 bytes changed from disc lie inside
the resolver's known header-write footprint; no other resource byte differs.
The200,716 distinct resource/directory bytes are identical across both captures.

This is original **demo** runtime evidence. The launch halfword8001EDEC is1,
with home17/New York and away5/Dallas; the currently supported native exhibition
snapshot uses launch0. The dumps were not paused at640D8 return, and whole RAM
differs in198,685 bytes between them. Acquisition atomicity, instruction timing,
rendered phase and input delivery are not established by stable resource bytes.
The new native resolver comparison below uses these saved captures; there is
still no gameplay sampler comparison or new live capture in that verification.

Private captures: .local/verification/gameplay/runtime-20260830/
demo-gameonly-ram.bin and demo-gameonly-followup-ram.bin. The independent report
and source-oracle comparison are audit_b/runtime_demo_mocap_comparison.json and
compare_demo_mocap_runtime.py. Original material is never included in commits.

## Evidence limits

The private synthetic oracle has23 cases with two passes each, covering all132
owner instructions while stubbing only the file-loader return. Twenty fit the
raw-file contract; two pre-relocated cases and one out-of-file address wrap are
source diagnostics. Eighteen historical native-guard proposals in that oracle
are specifications, not test passes; implemented guard tests are reported below.
The earlier actual-file
oracle covers126 post-loader instructions, all168 pointer outputs and unique
header mutations, plus second-pass idempotence. Full132 remains the denominator.

Evidence is under .local/verification/gameplay/audit_b/: the resolver contract,
source/lifetime references, synthetic vectors/results, actual-file results and
runtime comparison. None is a claim about a native gameplay implementation or
the ordinary-exhibition handoff from Team Select.

## Native verification and integration

`tests/gameplay_mocap_tests.cpp` uses only invented data. Its2793 checks cover
normalization quirks, alias identity/order, optional and unequal channels, zero
directory offsets, overlapping read-only directories, shared/backward/unaligned
data targets, the168-header capacity boundary, atomic guard failures, and retained
resource generations. Source count wrap is preserved and commented: count0 with
flag09 becomes255 and count128 with flag08 becomes0. These are not clamped or
silently repaired. Original hardware-address overflow and pre-relocated input
remain outside the explicitly bounded raw-file interface.

The private `compare_native.py` invokes the compiled C DLL and independently
executes the original GAMEONLY instructions with only the29BFC file-return hook.
It reruns all23 source cases twice (20 accepted raw encodings,3 source-only
diagnostics), plus64 randomized raw resources twice. All132 owner PCs execute.
It compares the native projection of all200044 actual-file bytes and672 output
directory bytes against the original owner and both saved demo captures, after
revalidating all seven code anchors (3876 bytes) in each capture. All145 unique
headers and five backward targets agree. The C++ file loader additionally agrees
on all158 nonnull references and preserves the prior owner after a failed file
replacement. Debug and RelWithDebInfo receipts retain hashes and scope limits.
This is native resolver evidence, not instruction-identical binary matching.

The C parser returns `NBA97_GAME_MOCAP_OK` on success; other named result codes
are native guard failures. Output and input are disjoint, and failure leaves the
output unchanged. `decode_gameplay_mocap` and `load_gameplay_mocap` return a
`GameplayMocapResource` shared pointer. Hold that owner for any borrowed index,
header or byte reference. Retained owners survive replacement; a fresh load makes
a fresh generation. No player/frame payload interpretation is exposed yet.

Integration needs the two new implementation files in the relevant native target
and a CTest target built from the new test plus both implementations (C99/C++17,
include directory `src`). Runtime loading belongs at the original2DB90→640D8
boundary before659F0, using privately extracted raw `ZMOCAP.BIN`; it must not
replace Create Player's six-clip parser. Caller teardown, the2E0EC rebuild lifetime
and actual gameplay sampling remain separate unresolved integration work.
