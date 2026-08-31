#ifndef NBA97_GAME_NET_TRANSFORM_H
#define NBA97_GAME_NET_TRANSFORM_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete GAME2DC88 (63 instructions, no children). Only access/user/budget
 * are used. The checked original-address and byte-knownness contract is exactly
 * Nba97PlayerFrameContext's; no camera value or fixed global is snapshotted or
 * invented. Reached writes remain published after refusal. The call is neither
 * resumable nor atomic, so clone the complete retained memory owner externally
 * if transactional host publication is required. Source code/private stack
 * cannot alias visible allocations. */
int nba97_game_net_transform(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
#ifdef __cplusplus
}
#endif
#endif
