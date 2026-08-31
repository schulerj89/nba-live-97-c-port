#ifndef NBA97_GAME_BALL_RELEASE_H
#define NBA97_GAME_BALL_RELEASE_H
#include "game_tipoff_phase.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameBallReleaseResult {
    NBA97_BALL_RELEASE_DIVZERO=-6,
    NBA97_BALL_RELEASE_DIVOVERFLOW=-7
};
/* Complete GAME58610 and its actual2AB70 calls. Input pointers are the real
 * thrower a0 and receiver a1, supplied by5BC34 or another proven caller.
 * Reuses the live checked address/knownness contract of game_tipoff_phase.h;
 * c->call is NOT used. Actual B8198/B81B0/B81C8 table data, player/status inputs,
 * ball reference and shared RNG must be mapped, never defaulted or regenerated.
 * All CPU writes occur in original order; aliases and prior mutations survive
 * refusal/source BREAK. Division traps identify the actual BREAK PC in receipt.
 * A completed release leaves the ball loose, with intended receiverFDBD2 and
 * velocities written. It does not advance gravity, catch the next pass, or
 * complete first possession. No renderer/audio/device implementation is needed
 * by this owner. State/context and receipt must not overlap. */
int nba97_game_ball_release(Nba97GameTipoffContext*,uint32_t thrower,uint32_t receiver,
                           Nba97GameTipoffReceipt*);
#ifdef __cplusplus
}
#endif
#endif
