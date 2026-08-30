# Bounded controller-map handoff

This implements FEONLY80061674 as a pure C function and a read-only C++ fixed-slot
adapter. It is a prerequisite for a match snapshot, not a gameplay launch or an
integrated live-controller lifetime. No original data, memory-card I/O, overlay
execution or model/rendering changes belong to this boundary.

## Source contract

The77-instruction owner clears36 bytes (0x24) at each of eight controller records,
stride0x78. Control maps occupy59 bytes at record+0x3c. The24-byte gap and final
byte are untouched. The semantic C structure separates the two known fields
rather than fabricating a complete original record.

The selector is loaded unsigned then explicitly sign-extended before its test.
All negative byte values skip copying and retain the previous live map when
the force argument is zero. This includes the ordinary FE/FF sentinels. For a
nonnegative selector, any nonzero raw validity byte selects the saved59-byte map;
zero selects the default map at800C1CD8. A nonzero force argument selects defaults
regardless of selector/validity. All paths clear the statistic prefix.

Retail has no upper-bound test before positive profile indexing. The native API
refuses selectors20..127 atomically unless force overrides them. This bounded
guard is not a claim about original malformed-input behavior. Pointer failures
also leave the output unchanged. The clear helper8008A944 is used only for the
aligned36-byte call contract; its full general behavior is not credited here.

## Native interfaces

recovered/match_controls.c owns the decision/copy/clear operation. Its provenance
output labels each map as retained, default or saved; these labels are native
diagnostics, not new retail state. Defaults and prior maps must be supplied
explicitly. No bytes are embedded in the implementation.

match_controls.cpp builds the20-slot source from UserProfile v2 and returns owned
maps, provenance and selected stable IDs. Input profile order does not determine
slot placement. Missing slots represent cleared records with validity0. This
preserves the source case where a neutral controller retains a selector after
another controller deletes that profile. Duplicate IDs/slots and unmapped legacy
slot255 refuse; the existing v1 store reader assigns fixed slots before this API.
The adapter does not read assets, write saves or mutate source objects.

## Verification and remaining integration

The asset-free match_controls_core CTest covers196,608 signed-selector/validity/
force combinations, all eight distinct maps/statistic prefixes, last-slot atomic
failure, mixed selectors, cleared slots, stable IDs, forced defaults, retained
maps across calls and owned outputs after later profile mutation.

Private raw-MIPS execution matches570/570 independent cases with no clear/copy
semantic hooks. It also verifies the untouched gap/tail in synthetic original
records, every raw validity byte, all negative selectors and repeated retention.
Another864 out-of-table selector cases confirm atomic native refusal. These are
separate evidence tiers: isolated instruction execution does not establish live
original runtime equivalence. The existing user_setup.json entry for80061674
retains all77 instructions and zero credited instructions; no second denominator
is created. The general81-instruction clear helper is not fully reconstructed.

Checkpoint validation:40/40 CTest tests pass in Debug and RelWithDebInfo;57/57
Team Select/User Setup captures repeat in each configuration without changes to
the preceding host checkpoint. Private runs: Debug173014-2d2d927c and
release173108-1aafafe6 on2026-08-30. Logs use .local/logs/match_controls_*.
Create Player rendering and persistence code are unchanged from the preceding
27/27-capture checkpoint. Existing metadata credit stays unchanged.

Before wiring the host, establish live maps across cold entry, User Setup return
and successive matches. The source28800 calls61674(1) only when resident word
80021EE4 is zero; resetting on every User Setup entry would erase valid live
maps. Fresh FEONLY data contains zero, and360D4 later writes one; no direct reset
writer was found in the FEONLY/GAMEONLY cross-reference audit. Extract the59
default bytes into a private purpose-specific pack and use the proven cold-entry
boundary. Other control editors and indirect mutation paths require their own
ownership audit.

Then create a semantic ordinary-exhibition match snapshot from current rosters,
settings, selected teams/profiles and these maps, preserving unsupported fields
as explicit pending dependencies. Original probes at61674 entry/return compare
8001EF7C length0x3c0, selectors80021DDE length8 and profile controls at
80020C1C length0x870. Validate backing before reading. Gameplay remains disabled.
