# Animation queues

`game_animation_queue.c/.h` recovers `56CE0` and its actual secondary `56C84` and primary `56C28` queue writers. The wrapper executes secondary first. Each channel independently checks its signed forced-motion lock (`4C` or `48`); a nonnegative lock prevents that channel from reading or changing its queue.

An unlocked channel searches four halfwords for **exactly** `FFFF`. Other negative halfwords remain occupied here, although the frame consumer treats a negative queue head as empty. A full queue silently drops the request. On an available slot, the source stores the low 16 request bits and low eight blend bits. Unless this is the last slot, it writes the next halfword to `FFFF`, leaving its old blend byte and every later entry untouched. The channels can therefore diverge; neither drops nor divergence are corrected.

Queue heads `70`/`78` reuse `Nba97GameAnimationState`. The additional halfwords and eight auxiliary bytes live in the shared `Nba97GameAnimationActor` declared by `game_animation_advance.h`. Every field has explicit knownness. Unknown fields bypassed by a locked channel remain unknown. A reached unknown lock or queue slot returns `NBA97_ANIMATION_UNRESOLVED`; no empty sentinel is invented. Queueing does not resolve a motion resource or mutate actor state `1A`.

The result contains the complete candidate state, original store count and write-footprint masks. Apply only written fields with their knownness. Calls are atomic, leave output unchanged on failure and support overlapping input/output storage. Resources are unnecessary for this owner.

## Verification and integration

Private evidence is in `.local/verification/native_completion/animation_advance/`. Both strict `/W4 /WX` native builds pass the public synthetic suite, including every one of the 65,536 signed primary lock values, independent channels, full queues, sentinel truncation, stale entries, request truncation and unknown fields. The raw original-MIPS differential executes 1,200 queue cases per build and all 66 source instructions, including delay slots and both actual child functions. It compares all 244 entity bytes, write footprints and the number of original stores. This is final-state and store-count evidence; the compact effects contract does not claim to expose a per-store event trace.

The actual mocap replay additionally queues 78 and 79 after original `56B78` switches through 39 and 77, then compares full `579FC` advances. See `game_animation_advance_workflow.md` for that bounded replay and the remaining physics/caller boundary.

The build owner should add `game_animation_queue.c`, `game_animation_advance.c` and their two test executables. Caller code supplies the current owned actor, full raw request and blend argument. It must apply the queue result before the next queue call or animation advance. No host, CMake, UI or Git files are changed by this recovery.
