#ifndef NBA97_GAME_COURT_RESOURCES_H
#define NBA97_GAME_COURT_RESOURCES_H
#include "game_text_pools.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameCourtResourceProgress {
    size_t accesses,events,stores,allocations_completed,callbacks_completed;
    uint32_t stopped_pc,stopped_address;
    Nba97GameTextPoolProgress allocation;
    uint8_t completed;
} Nba97GameCourtResourceProgress;

/* Original479B8 after-load tail48A4C..48D28. loaded_court is actual29BFC's
 * returned source address, not a native pointer or invented heap location.
 * Owns header/group relocation, live global cursors, both edge-list allocations
 * and per-bank edge packet initialization, including9C274's reached zero arm.
 * Calls the concrete shared90160/901EC allocation owner; its9027C callback
 * must perform real allocation effects (see game_text_pools.h), or refuse.
 * Court flags are0, not the text-pool caller's20. Payload is never zero-filled.
 *
 * Source memory includes actualFEBE4/102C84/FC964/FEDA0/FEDA4/10B60C/DCF10,
 * resource, allocator and line storage. All arithmetic wraps32 bits; signed
 * loop tests and repeated reads preserve aliases. Reached bytes validate
 * bounds/alignment/knownness, not unreached payload. No source-count repair.
 * The journal includes all tail stores plus nested90160 stores/9027C events;
 * allocator callback's internal stores belong to that owner. access_budget
 * limits tail data accesses; nested wrapper is finite and its allocator needs
 * its own budget. All prefixes survive refusal; clone memory and allocator
 * state for atomic publication. Not resumable. Existing pool memory/lifetime/
 * alias contract applies; code/source-stack aliases are excluded.
 * This is not whole479B8, file selection/loading, textures, live camera, or a
 * connected frame. The source epilogue only restores its private stack.
 */
int nba97_game_court_resources(Nba97GameTextPoolContext*,uint32_t loaded_court,
    size_t access_budget,Nba97GameTextPoolEvent*,size_t capacity,
    Nba97GameCourtResourceProgress*);
#ifdef __cplusplus
}
#endif
#endif
