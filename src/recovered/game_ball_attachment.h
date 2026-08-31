#ifndef NBA97_GAME_BALL_ATTACHMENT_H
#define NBA97_GAME_BALL_ATTACHMENT_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete native 57F5C/58120/581C0 and their 2D37C hand lookup. Actual
 * original globals, actor pointers and endpoint arrays must already exist.
 * Uses the player-frame access contract; math and child callbacks are unused.
 * return_v0 is the source register result, separate from the native status.
 * It is cleared on entry and populated only on successful completion.
 * Mode1 deliberately DOES NOT write ball height. Negative possession skips
 * every actor/ball access and returns32 (mode1) or signed possession (2/3).
 * Private ABI stack/output locals cannot alias visible input allocations.
 * Completed source effects survive refusal; do not retry a partial call.
 */
#define NBA97_BALL_ATTACH_BLEND UINT32_C(0x80057f5c)
#define NBA97_BALL_ATTACH_PRIMARY UINT32_C(0x80058120)
#define NBA97_BALL_ATTACH_SECONDARY UINT32_C(0x800581c0)
int nba97_game_ball_attachment(Nba97PlayerFrameContext*,uint32_t entry,
    Nba97GamePeriodValue* return_v0,Nba97PlayerFrameProgress*);
/* Full standalone 2D37C: destinations are original numeric addresses, not
 * host pointers. Stores X, Z, height sequentially, allowing genuine aliases
 * to affect later endpoint/actor reads. hand is the complete original a1,
 * compared to actor+9A bit0, not normalized to a boolean.
 */
int nba97_game_hand_endpoint(Nba97PlayerFrameContext*,uint32_t actor,uint32_t hand,
    uint32_t destination_x,uint32_t destination_z,uint32_t destination_height,
    Nba97GamePeriodValue* return_v0,Nba97PlayerFrameProgress*);
#ifdef __cplusplus
}
#endif
#endif
