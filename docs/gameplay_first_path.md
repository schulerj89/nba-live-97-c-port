# Smallest evidence-backed path after Team Select

2026-08-30 source audit with native Team Select, User Setup and an owned partial
ordinary-exhibition snapshot. GAMEONLY now has owned index projections and the
complete ordinary655B0 team-initialization effects before period setup; there
is no gameplay scene or loop. See team_header_workflow.md for the exact stage.
Original dumps, disassembly, art and raw tables remain private.
This is a sequence of bounded dependencies, not a broad gameplay rewrite.

2026-08-31 continuation: the snapshot now also contains the complete65328
controller effects before65DB0, preserving joined selection provenance across
acceptances. The gameplay motion resolver has a native immutable resource owner
and original-data comparison; its runtime loader/sampler integration remains
open. See game_controllers_workflow.md, gameplay_mocap_workflow.md and the
full [completion acceptance ledger](native_completion_acceptance.md).

## 1. Original User Setup, then a valid match snapshot

Team Select state3 returns1 to dispatcher8003F7C8, which invokes state5 owner
80037010. Retain its full1716-instruction denominator, including inline editing.
Its art is ZSET1/ba39 with cnt3/cnt2/cnt1 controller markers; do not use the
unrelated existing native profile-management page.

Eight physical controllers have local sides0 away /1 neutral /2 home.
Confirmed resident bytes80021EA6 encode2 away /0 neutral /1 home. No-tap display
uses physical slots0 and4, not0 and1. Left/Right change assignment, max5 per side.
Up/Down on joined controllers cycle optional profiles while skipping empty or
already claimed saved slots. Cross edits names; it is not a ready toggle.
Start succeeds with no active editor and no joined unresolved Start New profile.
There is no minimum-human or same-team prohibition in this gate. Select clears
profile choices and returns-1 without committing local side changes.

Implemented bounded tranche: recovered/user_setup C assignment/readiness/editor,
UserSetupSession ordered polling, original Help/warning/delete descriptors and
v2 fixed-slot profile transactions. Inline control1C is a signed vertical
baseline adjustment with zero width. The shared renderer has independent
original-font pixel checks. Original runtime comparisons remain pending.
Do not clear an unresolved profile to force readiness.

Success returns6. Dispatcher calls80061674,80046D24,8003E7A8 and finishes the
frontend.80061674(0) retains previous control settings for FE/FF profile
sentinels; it does not always install defaults.

The ordinary-exhibition snapshot now also implements80046D24's presentation
byte21DF4, including rejected shared-RNG draws and atomic native publication.
The accepted Start cue precedes this selection. No seed is installed at this
boundary. See match_presentation_workflow.md; original runtime acceptance and
the presentation byte's later gameplay interpretation remain pending.

The independent owner61674 (77 instructions) now has a pure C implementation
and read-only fixed-slot adapter; see match_controls_workflow.md. It clears36 statistic bytes
per controller; retain all59 live control bytes for FE/FF; copy saved controls
when validity is nonzero, otherwise defaults. A force-default bootstrap path
also exists. Current UserProfile v2 provides fixed slots,59 controls and raw
validity. MatchSession now owns the live maps, initializes the private defaults
once and atomically publishes partial snapshots; see match_snapshot_workflow.md.
A neutral controller can retain a selector to a deleted profile;
the cleared record selects defaults, so missing atSlot must not invent refusal.
Unsupported snapshot fields remain explicit; no gameplay is enabled.

Roster copies at80040900(home) and80040964(away) are12 records of0x6E bytes into
8002208C and800225B4. Sources are mutable team slot tables, not a stock database
reload. Special teams29/30 use8004E9D8. Player IDs>=493 route through8005FE14
to0x44-byte created records. The native created-player catalogue is currently
separate from roster membership: that boundary must be implemented or explicitly
refused, never silently dropped.

Short-roster boundary: the frontend copy helper does not guard null pointers
among its first12 records. Do not synthesize zero-filled original records and
call them equivalent. GAMEONLY80063D58 uses the resident roster count and aliases
remaining player references to record0;800655B0 clamps active count to12.
Preserve all15 current slot IDs/order, validate contiguous membership, and keep
unused raw copy bytes explicitly pending. The later FE8005DB34 count is the
first-null index, not an arbitrary count of nonempty holes.

GAMEONLY800655B0 also consumes team bytes+54/+57 as AI thresholds. The private
database pack's source_metadata[0..4] contains stock ranks; never mutate that
immutable save identity. The match snapshot now overlays freshly derived ranks
in its own copy before any future gameplay consumer can use it.

Settings must be copied by meaning, not a guessed contiguous options block.
The11 native option indices map to resident bytes21D86,21D7C,21D7D,21D7E,
21D7F,21D95,21D81,21D82,21D83,21D84,21D99. Rules occupy21D87..21D94;
custom-rule backup is21DA3..21DB0. Unknown bytes21D80/85 remain unknown.

Seven additional home/away setting pairs at21DE6..21DF3 feed GAMEONLY80065820.
The native snapshot now owns them once per fresh epoch, preserving the source
cold values1,1,0,7,5,0,0. Conditional gameplay projection and ordinary-exit67930
writeback have field-only C helpers; no gameplay loop invokes them yet. See
match_strategy_workflow.md. Their full UI meanings, imported warm state and
other extension bytes remain pending. They are not FrontendSettings options.
The controller handoff clears0x24 bytes per0x78-byte slot, then conditionally
copies59 control bytes; FE/FF skips that copy and requires retained live controls.

Acceptance: deterministic snapshots from current saved lineups/ratings, special
teams and a created player; all eight assignments and profile/control propagation;
cancel at each stage; synchronized original probes. First original stops:
800373B4 after state5 initialization,800374E4 after input aggregation,
800375EC/800375F4 readiness,8003768C successful return,80040900/964 copy boundary
and800409D8 after both snapshots. Dump80021D70 length0x170,80021EA6 length8,
80021DDE length8,8002208C length0xA50,80023AB0 length0xD00. Record registers and
source/destination fields before comparing.

## 2. Prove the gameplay overlay and asset lifetime

FEONLY loads GAMELOAD at801E0000; its entry word is801E1410. Loader801E136C saves
the resident block starting80015008, length from80015004 (0xF7A8), into801B0000.
It loads GAMEONLY at **80015000**, restores that resident block, then calls the
new entry word,80094828. No relocation table is used along this loader route.
The autogenerated overlay base800F9800 is not this execution mapping.

GAMEONLY clears BSS800D7BB8..8010B61B and enters80029994. FEONLY code is no longer
resident or callable merely because a same-valued address existed there.
GAMELOAD subsequently leaves too. Preserve portable data ownership explicitly;
do not embed overlay execution. Model/mocap relocation is a later, separate
format boundary.

Acceptance probes:801E13B0(before save),801E13B8(after save),
801E13CC(after load),801E13E8(after restore),80094828. Compare resident bytes,
entry words, code signatures and intended lifetimes. Derive and validate RAM
and separate1MiB VRAM signatures; never reuse an ASLR-dependent host address.

The next independent resource boundary is GAMEONLY640D8's ZMOCAP directory
resolver. See gameplay_mocap_workflow.md for its full132-instruction denominator,
signed/header-relative offsets, two84-entry optional directories, normalization
and resource lifetime. Its standalone parser can be verified before any gameplay
camera or clock is implemented; the existing frontend six-clip parser is not a
compatible substitute.

A later original automatic-demo observation confirms the motion resource's
bounded relocation result:200,044 resource bytes,145 headers, five backward
references and both84-entry directories agree with the original owner in two
dumps. The launch flag is1 rather than the native exhibition subset's0. This is
resource evidence only, not proof of the native handoff, full loader lifecycle,
camera, sampling, frame timing or a playable possession.

## 3. Deterministic court/player/camera scene

Recovered order:8002D8D4 ->8002DB90 initialization;8002DB68 ->80048D5C
->80052C20 scene/assets; later8002DC38 ->80068BF8 game loop. Asset orchestration
loads ZMOCAP.BIN, ZDOMTLST.BIN, home D/V and away E/W model resources, both S
resources, and home X/Y court resources. Court owner800479B8 constructs packet
and runtime arrays;800504A8 builds ten player instances with stride0xBCC.

The complete 165-instruction parent8002D8D4 is now native and connected to
main caller80029ADC. It proves the two buffer definitions, optional team-location
patch/restore, exact four-stage order, exit clear and eleven-wait presentation
tail while retaining the original late-flag and reloaded-index bugs. Its four
stage children remain explicit boundaries, so this does not change the scene or
playable-possession acceptance criteria below. See game_match_session_workflow.md.

After that owner returns, main's next call at `0x80029AE4` now enters the
complete loading-screen compositor `0x80029E58`. It loads `zloadscr.psh`,
looks up `LdS1`, synchronizes and uploads the same image to three framebuffer
locations, then releases the archive. The self-driving native diagnostic uses
the existing image owner and captures every incremental retained-VRAM
placement; it does not substitute retail art or imply that the four earlier
match-stage boundaries launch gameplay. The original silent null-archive path
and unchecked null-image behavior are retained. See
game_loading_screen_workflow.md.

The loader call inside that compositor and main's following `feload.bin` call
now both compose the complete retry wrapper `0x80029BFC`. The native visual
diagnostic deliberately returns null once for `zloadscr.psh` and twice for
`feload.bin`, proving the original backward branch before each call succeeds.
Four native before/after frames are pixel-identical because the wrapper itself
does not draw; its returned resources feed the loading compositor and FELOAD
handoff. Persistent failure still retries forever in source semantics—the
diagnostic budget reports that state without inventing a failure return. See
game_resource_loader_workflow.md.

Main's next call at `0x80029B08` now composes the complete nine-instruction
heap payload-size query `0x80090D60`. The successful diagnostic FELOAD service
publishes one retained allocation descriptor, the query uses the existing
recovered `0x80090618` heap-list search, and descriptor word `+0x14` supplies
the exact 5136-byte transfer size. Its native before/after frames are
pixel-identical because it only reads heap metadata. The original unchecked
null-descriptor read from low RAM `0x00000014`, wrapping address addition,
malformed-sentinel behavior, and live epilogue reload remain. See
game_heap_payload_size_workflow.md.

Before reuse of the frontend model libraries, compare counts, offsets, strides,
relocation, signed vertices, hierarchy/mocap data, matrices, camera fields and
packets at each boundary. Keep the existing verified Create Player path intact.
A first static scene acceptance needs numerical vertex/runtime/packet comparison,
deterministic camera and asset ownership, then matched frames. Appearance alone
does not establish compatibility.

## 4. Period initialization and tip-off

Period initializer80065DB0 sets phase800FDB90=0x81, delay0x78 and ball height0x5C00,
centers the players and selects animation0x27. Player entities start800FDCEC at
stride0xF4; entity10 is the ball at800FE674. Which phase consumer and tip-input
path lead to possession remain untraced.

Break after8002DB68,80048D5C,80052C20 and80067468. Dump8001EDF4 length0x188,
8001EF7C length0x3C0,80020B8C length0xA0 and800FDB4C length0xE7C. Watch phase,
quarter/clocks, ball-owner pointer800FDC48 and player/ball state. Distinguish
multiple calls to80065DB0 instead of assuming every one starts the same event.

## 5. One input, then the first playable possession

Only after the phase consumer is identified: capture one precise original input,
compare its first changed controller/entity/ball variable, implement that
bounded action and test it independently. Recover the exact first-possession
transition before adding shooting, passing, rules or broad AI. The controlling
button and first differing simulation field are currently unknown; do not infer
them from frontend controls or a different basketball game.
