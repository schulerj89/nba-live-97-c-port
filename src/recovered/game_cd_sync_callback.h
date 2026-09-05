#ifndef NBA97_GAME_CD_SYNC_CALLBACK_H
#define NBA97_GAME_CD_SYNC_CALLBACK_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCdSyncCallbackContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    uint32_t replacement_callback;
} Nba97GameCdSyncCallbackContext;

typedef struct Nba97GameCdSyncCallbackProgress {
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
} Nba97GameCdSyncCallbackProgress;

/* Original PsyQ CdSyncCallback at GAMEONLY 0x8009DBF8..0x8009DC0F
 * (6 instructions), called by main at 0x80029B44 with NULL immediately after
 * CdReadyCallback. It returns the prior callback from 0x800C57E8, then
 * unconditionally stores the raw replacement there. Internal CD_sync
 * 0x8009E740 reads this same slot at 0x8009E8BC; that use distinguishes it
 * from adjacent CdReadyCallback 0x8009DBE0, whose identical SDK body uses
 * 0x800C57E4.
 *
 * Other source call PCs are 0x8002B70C, 0x8002BB14, 0x80091F44,
 * 0x80091FC4, 0x8009D988, 0x8009F8F0, 0x8009F998, 0x8002D244,
 * 0x80092360, 0x80092760, 0x8009FE74, 0x8009FEF4, 0x800A0044 and
 * 0x800A0158. The source accepts NULL, unaligned, unmapped, or otherwise
 * invalid callback values without checking them, never invokes either
 * callback here, and returns a possibly unknown prior word even though the
 * replacement store still completes.
 *
 * Source bytes SHA-256:
 * a5f87457838841a01d7e1d1695406ed58575fa304d34b46e5ef4eb106cadddae. */
int nba97_game_cd_sync_callback(Nba97GameCdSyncCallbackContext*,
    Nba97GameCdSyncCallbackProgress*);

#ifdef __cplusplus
}
#endif
#endif
