# Native roster override format v1

This is a native-port format, not a reconstruction of a PS1 memory card.
The pure codec, canonical base identity and isolated database preparation are
implemented; disk save/load/recovery and normal-roster Reset are wired into the
Windows game host and tested with isolated saves. No older released format
exists to migrate yet; unknown required versions are refused. Original assets
and metadata are never embedded in the override.

All integers are unsigned little-endian. Maximum file size is1MiB and maximum
section count is64. Length checks precede section allocation. CRC32 uses the
reflected polynomial`0xedb88320`, initial/final XOR`0xffffffff`; this detects
corruption, not tampering or authenticity.

## Header and sections

| Byte offset | Width | Meaning |
|---|---:|---|
| 0 | 8 | `N97ROST` followed by zero |
| 8 | 2 | Major version1 |
| 10 | 2 | Minor version, preserved on rewrite |
| 12 | 2 | Header size64 |
| 14 | 2 | Flags0 |
| 16 | 4 | Complete file length, including final checksum |
| 20 | 2 | Section count |
| 22 | 2 | Reserved0 |
| 24 | 8 | Generation, at least1; transaction adapter must reject increment overflow |
| 32 | 32 | Canonical base-catalogue identity, schema below |

Sections follow at byte64 in strictly increasing tag order. Tags are four ASCII
characters from`A-Z`, `0-9` or`_`; duplicates are invalid. Each16-byte envelope
contains tag4, version2, flags2, payload-length4 and payload-CRC32. Flag bit0 means
required; other bits are unsupported. A final4-byte CRC covers the entire file
excluding that checksum itself. No gaps/trailing bytes are permitted.

`TEAM` v1 is required when present and contains only changed team lists. Each
64-byte record is team-ID2, capacity2(15), and15 player IDs of4 bytes. Team IDs
0..28 must be strictly increasing. The payload is a positive multiple of64,
at most29 records. `FREE` v1 is required when present and contains one404-byte
record: team29, capacity100, and100 IDs. Missing lists inherit their base lists.

The empty wire ID is`0xffffffff`, translated explicitly to native`0xffff`.
Other current IDs must be below`0x8000`; future created-player namespaces need
an explicit resolver and versioned required data. Slots must conserve the base
population exactly, including empties; duplicate nonempty base IDs are invalid.
This container-level check permits cross-team/free-agent transfers, while the
Re-order feature independently enforces unchanged per-team membership. Gameplay
capacity, contiguous-list and context eligibility checks belong to the adapter.

## Compatibility and publication boundary

- Unknown optional sections preserve tag/version/payload without interpretation.
  Their original byte payload survives; section order is canonicalized.
- Unknown required sections, major versions, header extensions or known-section
  versions raise`Unsupported`. A different identity raises`WrongBase`.
  Neither is permission to downgrade through a backup or overwrite defaults.
- Invalid lengths/checksums/descriptors raise`Corrupt`; invalid player population
  or unsupported IDs raise`InvalidRoster`. The storage layer must explicitly
  decide which errors permit offering a validated compatible backup.
- Future migrations need frozen version fixtures and explicit version routes.
  Accepting optional data is not proof that arbitrary future saves are compatible.
- An empty-override document is68 bytes; one changed team148; all team/free lists
  2,360. Reset is intended to write the empty override with a new generation,
  never delete a primary and accidentally resurrect an old backup.

The codec accepts an identity supplied by its caller. `RosterDatabase::load`
now computes the identity below and retains it with immutable original slots.
Tests join the codec to that real adapter using synthetic and private catalogues;
the separate store tests also exercise file publication and backup recovery.

## Canonical base identity v1

SHA256 is implemented as a bounded streaming hash from
[FIPS180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf), with no
cryptographic-library/runtime dependency. This is content identity, not file
authentication or a FIPS-validated module. Integers below are little-endian;
strings are a32-bit byte length plus exact stored bytes, without a terminator.
No host padding, pointers or container representation enter the stream.

Order is fixed; changing it requires a new identity schema and explicit save
compatibility handling:

1. ASCII `NBA97.ROSTER.BASE.V1` plus one zero byte.
2. Player count(u32); every player sorted by stable ID, including hidden/unlisted
   players. Each record contains:
   - ID, school index, original regular-stat index (three u16).
   - Postseason-stat index, jersey, position, height, weight-minus100,
     source-byte11 (six u8); source-word12(u16).
   - Ratings17 bytes; source metadata10 bytes; legacy games-played/started(u8 each).
   - Historical regular and playoff stat lines, each: validity(u8), twelve u16
     fields in order FGA/FGM/3PA/3PM/FTA/FTM/minutes/offensive-rebounds/
     defensive-rebounds/assists/fouls/blocks, then steals/games-played/
     games-started/ejections (four u8).
   - Seven strings: last, first, nickname, birthdate, birthplace, school,
     acquisition-method.
3. Team count(u32=29), then teams sorted by ID: ID(u16), five strings in order
   nickname/city/alternate/location/abbreviation, and source metadata20 bytes.
4. Original535 roster slots(u16 each, empty=`0xffff`), teams0..28 followed by
   free-agent team29; then the25 original special fallback IDs(u16 each).

The original database's source indices are logical behavior-bearing fields, not
pack offsets, and remain included. Current-session stats, derived ratings,
ownership caches, accepted overrides and screen drafts are excluded. File path,
container version, physical record order, padding and string-pool offsets are
excluded. A legacy pack that infers a different free-agent list is logically
different and therefore has a different identity; it is not silently remapped.

The synthetic golden fixture hashes14,062 bytes to
`99dedda8b2cb8658a724dcf8db0c8316cca1837598dd66069ca01e6b820fa3e4`.
Its independent Python struct/hashlib serialization is described by the exact
field order above; native tests retain the digest to detect schema drift.

`prepareSlotTable` refuses population changes and noncontiguous occupied lists,
builds all derived indexes off to the side and shares immutable metadata/base.
The no-allocation `swap` boundary is available for publication after durable
commit. It does not itself check writer generations or write a file; that remains
the transaction layer's responsibility.

## Tests

### Store boundary

`RosterSaveStore` owns an explicit `.n97rst` path with `.bak` and persistent
`.lock` siblings. Defaults/no-op create no primary; opening the store creates
its directory and lock file. Locks are never deleted while other instances could
be using them. Files must be independent regular files, not hardlink aliases or
directory targets. This is not a hostile-filesystem sandbox: cooperating writers
must honor the lock; external edits are rechecked immediately before replacement.

Primary and backup are fingerprinted using bounded chunked reads. Oversized
corrupt files do not cause size-proportional allocation, but hashing time is
proportional to actual file size. Missing/corrupt primary can recover a compatible
backup with an explicit recovery origin. Unsupported/wrong-base primary does not
fall back. Recovery requires repair even when the proposed order is unchanged.

All replacement state is prepared before publication. A unique temporary file
is written, flushed and verified, then renamed into place. A validated old primary
becomes backup first, so failed commits may advance backup while leaving primary
and live state unchanged. Retry accounts for this. Corrupt primary never becomes
the backup; incompatible/newer backup is protected from overwrite.

Windows uses
[FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
and same-volume MoveFileEx replacement with write-through requested. POSIX uses
file fsync, rename and [directory fsync](https://man7.org/linux/man-pages/man2/fsync.2.html).
The primary is not first deleted. API success is not certification of hardware/
filesystem behavior under power loss. After replacement, sync/reporting failure
returns committed with `sync_completed=false` and publishes new memory; callers
must not replay the old draft. Unknown replacement outcomes require reload.
Only owned temporary files are cleaned up; stale unknown temps are never promoted.

Run `nba97_roster_save_store_tests [private roster.n97db]` for isolated file
round trips, recovery, no-op, Reset, optional preservation, stale writers/locks,
16 precommit failures with retry, two postcommit failures, Windows sharing denial
and asset-alias rejection. These do not prove the game's Accept/error/retry UI
or a real power-cut guarantee.

### Codec checks

The separate Windows host proof is `python tools/verify_reorder_save_host.py`.
It uses unique local save paths and exercises real Accept/notice/Reset handlers,
including fresh-process reloads and failure paths. Default development saves are
at `.local/saves/rosters/default.n97rst`; `--roster-save <path>` selects another
roster set. Native Reset writes an empty override with a new generation; do not
delete the file to reset it, since a backup may then be recovered.

The format preserves unknown optional sections, refuses unsupported required
versions, and checks canonical base identity. No previous released format
requires a migration yet; no speculative migration is claimed.

Run`nba97_roster_save_tests` or the`roster_save_codec` CTest target. Local
`scripts/verify_reorder_rosters.ps1` and CI also run it without original assets.
Ten groups cover deterministic list round trips, a frozen independent wire
fixture, optional preservation, required-format/wrong-base rejection, nested
checksums, population validation, bounds, every truncation/single-bit change of
a148-byte fixture, and4,096 seeded checksum-repaired mutations.

The test logs actual byte sizes and requested C++ heap peaks for full-list encode
and decode. The16KiB heap gate excludes stack, allocator overhead, input objects,
catalogue, renderer and opaque extensions. It is not a whole-process RAM claim.
Disk failure injection, restart/recovery, writer conflicts and power-loss
guarantees require the separate transaction layer; these tests do not prove them.
