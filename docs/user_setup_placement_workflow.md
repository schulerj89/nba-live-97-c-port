# User Setup retained placement

The state5 owner80037010 now supplies portable text and marker placement targets
through `Nba97UserPlacement` in recovered/user_setup.c. UserSetupSession schedules
these mutations; renderUserSetup consumes them independently of current hardware
connectivity and the input owner's transient neutral-marker pulse.

This recovers the bounded state5 target/visibility contract. It does not implement
the shared text allocator, text-node deletion queue, both GPU primitive buffers,
or original VBlank/IRQ timing. Source2B830 queues duration1 movement;2D348 ticks it
before submission, reaching the target on the first tick and copying the other
buffer on the second. Marker31A0C writes both buffers immediately. The native
renderer uses the target at its next presentation; no general movement claim is made.

## Source ordering

Entry creates the included labels before the outer loop. Its initial99/-1
topology adoption recreates those labels, then moves all8 physical text groups
offscreen, in the new visual-row order, to `(700,66+18*row)`. All15 marker objects,
IDs18..32, move to `(700,66+18*object_index)`. Excluded old text groups survive
until the owner's exit; adoption does not erase editor or assignment state.
Included editor label recreation restarts the selected-character ten-tick tint.

The timed pass starts only at signed wrapping elapsed>6. A disconnected row
clears its logical state and hides text at `(700,name_y+18*row)` and its marker at
`(700,66+18*row)`. Connected rows run input first and place afterward:

| Topology | Physical rows | Text y / row stride | Marker IDs | Away / neutral / home x |
|---|---|---|---|---|
|0|0,4|88 /75|18..19|50 /180 /318|
|1|0,1,2,3,4|78 /27|20..24|80 /216 /350|
|2|0,4,5,6,7|78 /27|20..24|80 /216 /350|
|3|0..7|73 /17|25..32|106 /230 /380|

Connected text uses x256 when joined and700 when neutral. Marker y is73 plus
the row stride; the neutral movement pulse substitutes x700 for one completed
row pass. These values come from the source tables80024BC4..80024C40.

Help, capacity, delete and editor notices block before38820. The returning
controller completes that same tail without another input/repeat poll or
connectivity check. Later rows then use live connectivity in the retained old
topology. Only the next fresh outer iteration observes another topology sample.
An unplugged invoking controller can therefore remain displayed until a later
timed pass. Excluded logical-slot cleanup runs after all included rows and does
not issue placement calls.

Successful inline saves also complete the current tail and remaining rows before
36898 presents. A failed native durable write keeps the editor and opens its
explicitly native failure notice. Both retail terminal paths clear all text via
2C0F0(0), leaving marker placements alone. `deferMatch` is a native pending-page
policy: it restores included text existence while gameplay launch remains absent.

## Verification limits

Public session tests use source-derived numeric coordinates, all four topologies,
clock6/7 boundaries, neutral pulses, editor preservation, disconnected/modal
continuations and text-only terminal cleanup. They use no save files or private
assets. The isolated host capture adds12 controlled-clock scenarios, records
text and marker visibility masks, and checks original label/marker pixels with
fixed palette/title and independently varied targets. These are native adapter
checks, not original framebuffer comparisons.

Independent original-MIPS fixtures and compiled-C comparisons pass11,895
placement/tint assertions and are retained under
`.local/verification/team_select/audit_a/topology_visibility/`. They isolate the
placement calls and source tables; allocator history and runtime timing remain
separate. Seven tests of the extracted current host method also cover immediate
saves, consecutive saves, later Help, save failure and delete accept/cancel/failure,
using platform/persistence stubs without UI or files. Full owner/shared
denominators are unchanged and new instruction
credit remains zero. A physical state3/state5 walkthrough and synchronized
original frames, arrow-node history, audio and input cadence are still required.
