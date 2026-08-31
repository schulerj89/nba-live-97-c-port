#ifndef NBA97_GAME_BALL_FRAME_H
#define NBA97_GAME_BALL_FRAME_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Full49300 ball/reflection and49D34 ground shadow. Uses the same actual
 * address/byte-knowledge and retained math contracts as the player pass.
 * child is unused. Geometry kinds are NBA97_PROJECTION_* and NBA97_FRAME_*.
 * Required packet templates/UVs/VRAM come from4D490; no default resources,
 * camera, ball attachment, bank or ordering state is supplied here.
 * Refusal preserves every preceding CPU/math effect. Private ABI stack and
 * its dead FLAG/IR0 result stores cannot alias any visible allocation.
 */
int nba97_game_ball_frame(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
int nba97_game_ball_shadow(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
#ifdef __cplusplus
}
#endif
#endif
