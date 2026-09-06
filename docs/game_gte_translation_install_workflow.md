# GTE translation installation recovery

`game_gte_translation_install.c` owns the complete GAMEONLY routine
`0x80055F44..0x80055F5F`: 28 bytes and seven instructions. The fresh Ghidra
listing `game_80055f44.txt` has instruction SHA-256
`78671a48aee9c49116cf1dea240b731b38a17237d4975e6421f76dc727e5dd25`.
Fourteen source call sites are known, including the recovered camera-frame
transform call at `0x8005120C`. The routine has no children. DuckStation's GTE
control-register implementation independently confirms that controls 5, 6,
and 7 retain all raw 32-bit values.

The owner reads words at A0+0x14, A0+0x18, and A0+0x1C into T0, T1, and T2.
All three loads complete before the first control write. It then copies those
raw words and their per-byte knownness into retained GTE controls 5, 6, and 7.
Control 7 is the delay slot of `JR RA`, so that write remains visible when an
unknown RA prevents native continuation. No value is invented in V0. Every
other GPR, HI/LO, GTE control, and guest-memory byte remains unchanged.

The C99 owner treats A0 as a wrapping 32-bit guest address and uses validated
little-endian retained-memory regions. Separate access and control journals
make the three-read/three-write order and every operation-budget prefix
observable. It contains no guest-to-host pointer cast, CPU/GTE interpreter,
retail table, or runtime asset.

`game_gte_translation_install_adapter.cpp` binds the owner to the existing AQ
camera-frame-transform event at call PC `0x8005120C`, delay PC `0x80051210`,
entry `0x80055F44`, one argument, and known RA `0x80051214`. The binding owns
an explicit 32-word retained GTE control bank, copies the owner's exact full
machine prefix back for success and failure, and forwards every other AQ child
through its typed fallback callback. The actual AQ integration reaches AZ and
its following reference callback observes the installed translation controls.

The focused executable performs 501 always-active checks. It covers exact PCs,
addresses, widths, and operation order; raw upper bits and signed extrema; all
16 byte-knownness masks; all GPRs, HI/LO, SP, RA, and untouched controls; every
budget from zero through six; the final delay before unknown RA; unknown,
unaligned, unmapped, and wrapping A0; aliased native backing; malformed late
load bytes with unchanged T2; invalid machine and control masks; overlapping
regions; truncated and absent journals; no guest writes; null arguments; and
deterministic repetition. The natural AQ executable performs 204 always-active
checks for exact event metadata, RA and machine guards, nested budget prefixes,
typed fallback behavior, child failures before and after AZ, and reference
observation of controls 5 through 7. Both executables compile as strict C99 and
C++17 with `-Wall -Wextra -Werror -pedantic-errors` and generate all fixture
state at runtime.

The independent original-instruction differential passed 3,584 cases across
all seven source PCs, all 32 mapped fixture bytes, all 32 GPRs plus HI/LO, all
32 retained GTE controls and their byte masks, operation budgets zero through
six, and the unknown-RA delay prefix.

Production dependencies are the shared mapped-memory/full-machine types and
the recovered AQ camera-frame-transform event contract. The GTE rotation owner
and rotation-matrix builder compose on one retained GTE bank.

Gameplay shown: **NO - no direct visual effect**. This routine changes retained
CPU-visible GTE translation controls; later projection and rendering work must
consume those controls before pixels can change.

Native integration composes the recovered matrix builder and
rotation installer before this owner, transporting all 32 controls and their
byte masks through one retained bank. The actual camera caller clears the three
translation words before installation; the following typed reference service
observes that exact zero state. Both complete and budget-limited compositions
preserve earlier rotation writes. Native capture asserts these transitions and
matching CPU-only frame hashes; no rendered gameplay is claimed.
