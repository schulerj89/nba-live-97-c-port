# Native user profile format v2

This is native persistence, not a PS1 memory-card image. Version1 remains
readable. A changed save writes version2; older binaries cannot read it.
Loading, cancelling and unchanged-name acceptance never migrate the file.
Keep backups when moving between binary versions.

Integers are little-endian. Files are bounded to16KiB and20 profiles. The40-byte
header is followed by16-byte directory entries (tag4, offset4, byte-size4,
record-count4). Sections may be reordered but cannot overlap the directory or
each other. Unknown sections/versions are refused, without loading an older
backup over an unsupported primary.

| Header offset | Width | Meaning |
|---|---:|---|
| 0 | 8 | N97PROF followed by zero |
| 8 | 2 | Major version1 or2 |
| 10 | 2 | Minor version0 |
| 12 | 8 | Native generation |
| 20 | 4 | Directory entry count |
| 24 | 4 | Complete file length |
| 28 | 4 | CRC32 of complete file with this field zero |
| 32 | 8 | Reserved, writer emits zero |

CRC uses reflected polynomial0xedb88320, initial/final XOR0xffffffff. It detects
corruption, not authenticity. Generation overflow refuses mutation.

PROF records are48 bytes: ID8, creation time8, updated time8, name length1,
name bytes, zero padding. Names are1..13 printable ASCII bytes, preserved exactly
including legacy spaces. Duplicate names are case-sensitive; duplicate/zero IDs
are invalid. Times are native Unix seconds.

STAT records are72 bytes: ID8 then16 uint32 native statistics in this order:
games, wins, losses, points, field goals made/attempted, three-pointers made/
attempted, free throws made/attempted, rebounds, assists, steals, blocks, turnovers,
fouls. They are not claimed to reproduce the unknown retail record prefix.
Records associate by ID, not section order. V2 requires complete STAT coverage.

V2 adds required CTRL records, also72 bytes: ID8, fixed slot1 (0..19),
raw controls-valid byte1, controls59, reserved zero bytes3. Slots must be unique.
Every profile has exactly one CTRL; orphan/duplicate records are rejected.
The validity byte is preserved as a byte, not normalized to boolean.
V1 migration assigns slots in PROF order and initializes controls/validity to zero.
Deletion leaves a slot hole; later creation may reuse it without moving other IDs.

The exact editor adapter preserves ID, creation time, statistics and controls for
existing acceptance. New acceptance resets those fields only when the recovered
existing-editor flag requests clearing. No-op acceptance keeps bytes, generation
and timestamps unchanged. The earlier normalized native CRUD remains separate.

Writers acquire a sibling .lock and compare both primary and .bak to the bytes
loaded before writing .tmp and replacing the primary. A stale view refuses.
Unknown or newer-generation backups are protected. Missing/corrupt primary may
recover from a supported backup; repair does not overwrite the good backup with
corrupt primary bytes. Failed precommit writes roll back in-memory mutation.

Tests use manually packed wire fixtures and isolated run directories only.
Windows temp-open, sharing/lock, stale-writer, recovery, rename/delete/reuse and
retry paths are tested. POSIX replacement and power-loss/ambiguous OS failure
outcomes have not been validated; this is not a universal crash-safety claim.
