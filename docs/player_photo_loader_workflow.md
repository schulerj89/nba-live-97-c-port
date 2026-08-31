# Native portrait checksum completion

Actual View Player requests now use the original Z1PORT.IDX/BIG record before
publishing a decoded PNG. The loader retains a checksum acceptance even when
the PNG stage fails, discards cancelled work before source effects, and calls
the recovered checksum gate on the same persistent music input consumed by
the host's routing update. This is an integrated request/completion path;
full31A88 transition fading and volume reduction remain separate work.

## Ownership and record selection

`PlayerPortraitArchive` owns immutable index and archive vectors through a
`shared_ptr<const ...>`. Index loading uses the proven original whole-file
checksum decision from `frontend_resource.c`. A present CRCF trailer must
match; the retail optional-trailer rule is retained. The source12-byte shrink
leaves the inner checksum intact. Reserved-inclusive table sizing and every
raw record extent are checked before an archive can be published. Malformed
negative player IDs, undersized records and out-of-bounds spans are explicit
native errors, not silently repaired original inputs.

Record mapping follows310D8: nonnegative logical player below count selects
logical+1; otherwise it selects reserved0. The host no longer chooses fallback
because a PNG file is missing. Thus logical492 requests physical493, while
logical493 or a created-player ID beyond the original count requests0. The
separate extractor correction restored the final original PNG.

The host retains one immutable portrait archive while it runs. Each worker
captures its own shared ownership, so replacing a request or leaving a card
cannot invalidate its raw slice. Source F9418/Z1PORT ownership is distinct
from F84C8/Z1COOL; no portrait allocation or dummy handle is published into
the Cool Facts field. Resident caching is a native platform adaptation, not
a claim about exact PS1 allocation addresses or CD read timing.

## Completion ordering and retained source behavior

The checked request overload is
`request(archive, logical_player, png_directory)`. A worker first verifies the
requested raw slice using the original CRC. Only an accepted slice reaches
the existing PNG decoder. Its result retains `RawChecksum::Accepted` through
a PNG exception, wrong dimensions or incomplete pixels. A checksum rejection
returns a failed result without decoding PNG data.

`poll(wait, before_publish)` runs on the caller/UI thread. Its ordering is:

1. Discard a stale generation. Clear the result's raw acceptance and archive
   reference, preserve the newer request, and invoke no source callback.
2. For current raw acceptance, invoke `before_publish`, even if the PNG failed.
3. Complete the existing recovered photo visibility state, then return pixels
   for host publication only if the PNG is valid180x156 RGBA.

The host callback calls `archive.acceptChecksum(record,
&frontend_music_inputs_.selection_blocked)`, executing the proven30EFC gate on
live state. The raw bytes remain immutable, and the callback rechecks them.
It logs the previous F9720 value, accepted physical record and PNG outcome.
No worker touches music, RNG, UI or visibility state. Callback code must not
reenter its loader.

This preserves30E78's important order: checksum success clears F9720 before
visibility publication, and PNG failure does not undo that clear. Original
texture decoding was synchronous inside the source callback; native PNG work
runs on a worker before the UI consumes its result. Exact callback latency
and CD timing are not claimed. A render-complete or animation-end event cannot
substitute for the checksum callback.

The source cache quirks remain: requesting the same physical record does not
reload or hide it; changing records hides only the photo, retaining an already
visible city strip. Reset/entry clears the cache. A different immutable archive
requires a caller reset before requesting a coincident physical record. The
existing image-only overload is retained for legacy callers/tests and returns
`NotChecked`; it can never invoke the checksum acceptance callback. The actual
host View Player path always uses the checked overload.

On corrupt raw data, the native loader reports an asset error and keeps music
blocked rather than simulating the original queue's retry loop. It does not
claim original393F0/38E84 retry or allocator equivalence. Native generations
provide bounded cancellation: one worker plus the latest pending request.
They are not reconstructed PS1 callback identities or CD scheduling.

## Shared music state and remaining composition

`win32_main.cpp` now owns persistent `frontend_music_inputs_`, which is passed
by reference to the existing routing update and used by the portrait callback.
This replaces the previous fresh zeroed input each frame; there is no second
unconnected block flag. No frozen playback API changed.

The subsequent31A88 begin/end composition must write this same state and
retain its original saved-volume and repeated-entry quirks. At this checkpoint
volume still refreshes from the configuration option each update and ED2AC
still comes from actual View Player/child state24. Persistent volume ducking,
transition fade ownership and F84C8 Cool Facts resource composition are not
claimed. No gameplay Pause state or automatic pause-song request was added.

The changed host callsites are `loadSelectedPlayerCardAssets`,
`updatePlayerPhoto`, persistent music input storage and its existing update.
All normal/child photo entry and player-cycle requests use those shared paths.
Other host transitions, speech, playback, recovered resource owners and
original photo visibility C were left unchanged.

## Validation and build integration

Private receipts are in
`.local/verification/native_completion/player_photo_loader/`:

- `build_archive.py`: bounds, reserved record mapping, original index shrink,
  immutable ownership, rejection cases and all494 actual raw portrait CRCs.
- `build_loader.py`: unchanged legacy viewer tests, checksum/cancellation/
  lifetime tests, and40 repeated runs of deterministically gated worker cases.
- `build_actual_probe.py`: current raw archive and actual WIC PNG decoder for
  all494 portraits. Each UI callback cleared the same input used by the
  recovered routing owner; all494 blocked selections subsequently proceeded.
  An explicit fixture override kept the shared16-bit RNG unchanged. The probe
  opened no audio device or UI and also syntax-checked current win32_main.

Tests cover valid raw/bad PNG, bad raw/no PNG decode, last record/fallback,
duplicate requests, newest-only replacement, exit cancellation, retained worker
ownership after reset, UI-thread callbacks before visibility, and unchecked
legacy results never producing checksum effects. The new C++ sources and
tests compile with MSVC/O2/W4/WX. This is native integration evidence using
previously proven original C owners, not new full-GPU or frontend proof.

CMake now includes both C++ owners in the host and legacy photo test, with
the resource CRC owner linked into the standalone tests. The new archive and
checksum-loader targets are included; loader tests retain Threads linkage.

The integrated checkpoint passed all84 CTests in Debug and RelWithDebInfo,
plus actual host self-tests and View Rosters/Re-order/Trade/Sign/Release
captures in both builds. Compared with the previous42197ed executable,
720 frames are byte-identical. Four Release return-menu frames per build use
the existing wallclock-seeded ZCARD choices; their logged card indices differ,
but every pixel outside the eight authored69x63 artwork spans is identical.
These eight frames are not counted as whole-frame matches. All44 observed
portrait callbacks across the two builds report raw checksum acceptance
before PNG publication. Real save/config file hashes and timestamps did not
change. Receipts are private under `checkpoint8-host-v2`; they do not establish
original GPU timing, full31A88 parity or natural gameplay entry.
