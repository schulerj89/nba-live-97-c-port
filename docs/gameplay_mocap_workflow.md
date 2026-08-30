# Gameplay motion resource: bounded resolver contract

2026-08-30. This is an audited resource boundary toward gameplay, not a native
scene, animation sampler or playable possession. No gameplay motion parser is
implemented yet. Original files, invented source-oracle fixtures, RAM snapshots
and comparison scripts remain ignored under .local/verification/gameplay/.

## Source ownership

| GAMEONLY function | Scope | Accounted / full instructions | Dependencies and evidence | Remaining uncertainty |
|---|---|---|---|---|
|800640D8 |Load and normalize two motion directories |0 /132 |29BFC;23 synthetic cases over the full owner, two passes; actual-file post-loader comparison |Native parser/ownership, other callers and playback |
|80029BFC |Retry file loading until a nonzero pointer |0 /17 |941C8; callsite and filename/argument checks only |File/device/allocation failure behavior; not implemented by the oracle's loader stub |
|800642E8 |Release resource and clear21490 |0 /17 |90698; source audit only |Native teardown and stale directory users |

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

## Next native implementation boundary

A focused portable C parser can produce an immutable index: two84-entry optional
references, at most168 unique headers, file/header/data offsets, and both raw and
normalized flags/timing/count. A C++ resource owns the input bytes and index and
publishes only after validation. Reuse the cached index for the same resource;
decode fresh bytes for a new resource. No original address becomes a host pointer.

Native bounds checks must be labeled as guards, not retail rejection branches.
Validate directory/header extents and4-byte alignment, signed in-file targets,
and output capacity. Permit exact directory/header aliases, different headers
sharing data, nulls, unequal counts and unrelated flag bits. Reject overlapping
distinct headers and headers overlapping control words/directories, which would
otherwise let original in-place writes alter later traversal. Do not infer a
payload extent or joint/frame stride from this resolver alone.

For the first immutable raw-file API, reject pre-relocated flag20 input. Its+8
word already holds an absolute PS1 pointer; it is a different encoding, not another
relative offset. A future RAM-import API would need an explicit encoding and
validated source base. Source normalization evidence includes those inputs even
though the proposed disk API deliberately excludes them.

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
There is still no native gameplay parser or sampler comparison.

Private captures: .local/verification/gameplay/runtime-20260830/
demo-gameonly-ram.bin and demo-gameonly-followup-ram.bin. The independent report
and source-oracle comparison are audit_b/runtime_demo_mocap_comparison.json and
compare_demo_mocap_runtime.py. Original material is never included in commits.

## Evidence limits

The private synthetic oracle has23 cases with two passes each, covering all132
owner instructions while stubbing only the file-loader return. Twenty fit the
proposed raw-file contract; two pre-relocated cases and one out-of-file address
wrap are source diagnostics. Eighteen additional malformed-input cases specify
future native guards; they are not native test passes. The earlier actual-file
oracle covers126 post-loader instructions, all168 pointer outputs and unique
header mutations, plus second-pass idempotence. Full132 remains the denominator.

Evidence is under .local/verification/gameplay/audit_b/: the resolver contract,
source/lifetime references, synthetic vectors/results, actual-file results and
runtime comparison. None is a claim about a native gameplay implementation or
the ordinary-exhibition handoff from Team Select.
