#ifndef NBA97_GAME_CD_READY_CALLBACK_H
#define NBA97_GAME_CD_READY_CALLBACK_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCdReadyCallbackContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    uint32_t replacement_callback;
} Nba97GameCdReadyCallbackContext;

typedef struct Nba97GameCdReadyCallbackProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t callback_global;
    uint32_t requested_callback;
    uint32_t previous_callback;
    uint32_t return_v0;
    uint8_t previous_callback_known;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameCdReadyCallbackProgress;

/* Original PsyQ CdReadyCallback at GAMEONLY 0x8009DBE0..0x8009DBF7
 * (6 instructions), called by main at 0x80029B3C with NULL after CdSync.
 * It returns the prior callback from 0x800C57E4, then unconditionally stores
 * the raw replacement there. Internal CdReady 0x8009E9C0 reads this same slot
 * at 0x8009EB78; that use distinguishes it from adjacent CdSyncCallback
 * 0x8009DBF8, whose otherwise identical SDK body uses 0x800C57E8.
 *
 * Other source call PCs are 0x8009D978, 0x8009FABC, 0x8009FC4C,
 * 0x8009FC80, 0x8009FE64, 0x8009FEEC and 0x800A0144. The source accepts
 * NULL, unaligned, unmapped, or otherwise invalid callback values without
 * checking them, never invokes either callback here, and returns a possibly
 * unknown prior word even though the replacement store still completes.
 *
 * Source bytes SHA-256:
 * 98c5f9f745cd61ca8a7268bf74d7dea2419d421b67d277c31d38f64b41113414. */
int nba97_game_cd_ready_callback(Nba97GameCdReadyCallbackContext*,
    Nba97GameCdReadyCallbackProgress*);

#ifdef __cplusplus
}
#endif
#endif
