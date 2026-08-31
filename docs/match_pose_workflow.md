# Owned match pose preparation

`prepareMatchRuntimePoses` joins the current canonical match records to actual
57B18 requests/foot stabilization and the530FC render-value sampler. It retains
the matching immutable mocap/ZHOTS/trig generation in its result. C++ owns the
record transaction; recovered C owns the game decisions.

The request and render spans have separate source owners.57B18 starts from
20BEC[0] and visits ten consecutive physical records.530FC starts fromFC654;
4D38C's store at4D418 sets that reference to physical entity0. The adapter
requires FC654's native reference explicitly. It never derives it from the
request pointer or a sorted render table. An independent source review caught
and corrected an initial bridge that incorrectly used one span for both.

The adapter stages all eleven represented entity records. Requests update the
ten records in their own span; packets and sampled values then come from the
separate render span. If the spans differ, untouched render records retain their
previous request fields, exactly as the source would. The result exposes both
physical mappings. No default pointer is supplied when either source is unknown.

Each request field keeps its original width and knownness. The foot callback
receives the address of a retained shared-resource variable and reads the
current candidate state, including leg changes already made by57B18. After all
requests and samples succeed, only changed request/cache/foot fields publish
back to the canonical records. Partially known untouched fields are preserved.
The output poses contain only twenty Euler triples and root height; unwritten
scratch markers/root words are not invented.

On a missing input, missing foot data or out-of-file sample, the result retains
the typed request prefix and completion counts, but the live records remain
unchanged. This is an explicit native frame transaction, not a claim that the
original owners rolled back. It must not be used to resume a half-finished
original request cursor blindly. The caller can inspect the failure without
publishing a misleading partial frame.

Public integration tests cover independent request/render spans, original
midpoint requests, inactive unknown B fields, actual foot callbacks, the
fourth-event foot lock and signed EC overflow, unrelated-byte preservation,
resource-generation mismatch, unresolved source pointers and atomic refusal
after a completed source prefix. Underlying original-instruction proofs and
sampling limits are in `gameplay_pose_workflow.md`.

Independent private `checkpoint7/pose-review-{Debug,Release}.json` compares
48 cases and480 rendered poses per build, including request starts0/1 while
the actual render start remains0. It checks657,216 known record bytes per
build after original65DB0,57B18 with its actual foot callees, and530FC with
actual conversion/blend callees. Cases include unequal channel counts, the
backward clip37 data, previous clips, weight65535, signed foot-counter wrap
and wrapped positions. All reached owners execute; no render span is changed
in the oracle to conceal the source's separate pointer. These are explicit
source-state fixtures, not a new emulator or visible-scene capture.

This is not the whole original render context, geometry/texture/camera setup,
screen draw, simulation loop or a playable possession. Model/court composition
must consume these values through its actual source owners rather than reuse
the frontend clip decoder or fabricate missing scene state.
