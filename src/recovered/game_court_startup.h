#ifndef NBA97_GAME_COURT_STARTUP_H
#define NBA97_GAME_COURT_STARTUP_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameCourtStartupEventKind {
    NBA97_COURT_STARTUP_STORE=0,
    NBA97_COURT_STARTUP_LOAD_29BFC,
    NBA97_COURT_STARTUP_SYNC_994F4,
    NBA97_COURT_STARTUP_FREE_90698
};
typedef struct Nba97GameCourtStartupEvent {
    uint32_t pc,address,value,argument[2],returned; /* returned meaningful only for LOAD. */
    uint8_t kind,width,completed;
} Nba97GameCourtStartupEvent;
/* Required synchronous source services. LOAD must execute actual29BFC,
 * including941C8, retry, heap/checksum/resize and file effects, and supply its
 * actual nonzero original payload address. SYNC requires actual994F4 effects;
 * FREE requires90698's exact-payload lookup and allocator effects. Return1
 * only after completion. No callback/default success/malloc/address invention.
 * Callbacks may mutate retained bytes/knownness, not mapping metadata,
 * journal/progress, event, or lifetimes. Neither pointer may escape. A refused
 * call may have effects: its original operation is NOT rolled back. */
typedef int (*Nba97GameCourtStartupIo)(void*,const Nba97GameTextMemory*,
    const Nba97GameCourtStartupEvent*,uint32_t* returned);
typedef struct Nba97GameCourtStartupContext {
    Nba97GameTextMemory memory;
    Nba97GameCourtStartupIo io;
    void* user;
} Nba97GameCourtStartupContext;
typedef struct Nba97GameCourtStartupProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address,filename,loaded_resource;
    uint8_t completed;
} Nba97GameCourtStartupProgress;

/* Exact two missing479B8 intervals around the EXISTING court texture owner:
 * select_texture:48744..487B8; select_geometry:48894..48A4C. The latter also
 * owns all four special-court gradient packets and reached9C33C/9C274 arms.
 * First complete select_texture, pass actual loaded_resource to existing
 * nba97_game_court_textures, then invoke select_geometry ONLY if that whole
 * texture owner completes. Pass geometry loaded_resource to the existing
 * nba97_game_court_resources owner. Completion here means ONLY the named
 * interval, not the texture loop, geometry tail, whole479B8 or natural entry.
 * Both loaders use flags0. There is no source team-index validation: raw
 * word21D74*4 wraps and the reached table pointer is read. Nonzero1EC94 uses
 * slot31; nonzeroDCF10 uses the actual source literal filename address.
 * DCF10 is read before both sentinel stores in stage1, and reread AFTER sync
 * and free in stage2. Callbacks may change selectors for the geometry load.
 * Original low24 packet links remain untouched/possibly unknown. The tag,
 * command, RGB and XY stores retain their individual order and knownness.
 * Retained text-memory contract applies, including possible native backing
 * aliases. Code/source-stack aliases are outside this adapter's domain.
 * Native access/journal bounds are not source limits. Reached unknown bytes,
 * alignment, storage, budget and service refusal retain exact earlier effects.
 * Not resumable/transactional; clone ALL memory/heap/GPU state for atomic use.
 * Invalid arguments leave progress untouched. Outputs require original
 * source-address provenance; a diagnostic or host pointer is not provenance.
 */
int nba97_game_court_startup_select_texture(Nba97GameCourtStartupContext*,
    size_t access_budget,Nba97GameCourtStartupEvent*,size_t capacity,
    Nba97GameCourtStartupProgress*);
int nba97_game_court_startup_select_geometry(Nba97GameCourtStartupContext*,
    uint32_t loaded_texture,size_t access_budget,Nba97GameCourtStartupEvent*,
    size_t capacity,Nba97GameCourtStartupProgress*);
#ifdef __cplusplus
}
#endif
#endif
