#ifndef NBA97_GAME_POSE_FRAME_H
#define NBA97_GAME_POSE_FRAME_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete stateful530FC and reached54FCC/55018 closure. This supplements the
 * existing value-only pose sampler: real context pointers, frame halfwords,
 * scratch writes and live cursors are retained. Only access/user/budget are
 * used. No source resource, map, entity, pointer or scratch byte is invented.
 * Memory/reference/knownness contract is the same as game_player_frame.h.
 * Failure retains its exact earlier writes, is not resumable and is not atomic.
 * Clone/rebind all retained state outside this entry when atomicity is needed.
 * Code/private ABI stack must not alias visible allocations. */
int nba97_game_pose_frame(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
/* Complete54FCC, including its at-least-once signed countdown and live map
 * reads. Source count/map indices wrap; no native permutation is substituted. */
int nba97_game_pose_convert(Nba97PlayerFrameContext*,uint32_t source,
    uint32_t destination,uint32_t map,uint32_t count,Nba97PlayerFrameProgress*);
/*55018 in530FC's actual unsigned-halfword weight domain. Six signed-halfword
 * reads precede all three output stores, preserving actual source aliases.
 * Joint marker/padding halfwords are not written. */
int nba97_game_pose_blend(Nba97PlayerFrameContext*,uint32_t first,uint32_t second,
    uint32_t destination,uint16_t weight,Nba97PlayerFrameProgress*);
#ifdef __cplusplus
}
#endif
#endif
