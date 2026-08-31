#ifndef NBA97_GAME_LINEUP_RECOVERY_H
#define NBA97_GAME_LINEUP_RECOVERY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameRecoveryTeam {
    uint16_t lineup[12];       /* Header+16, signed source halfwords. */
    uint16_t inverse[12];      /* Header+80; read live after each substitution. */
    uint16_t preferred[5];     /* Header+98, roster slots. */
    uint16_t recovery_count;   /* Header+66, wrapping increment. */
    uint16_t human_count;      /* Header+42. */
    uint8_t automatic;         /* Header+76, unsigned byte. */
} Nba97GameRecoveryTeam;

typedef struct Nba97GameLineupRecoveryState {
    Nba97GameRecoveryTeam team[2];
    uint16_t status[24];       /* Record1F7EC + slot*22h +20h. */
    uint16_t marker;           /* FDB8E. */
    uint16_t substitution_lock; /* FDB54. */
} Nba97GameLineupRecoveryState;

typedef enum Nba97GameRecoveryResult {
    NBA97_RECOVERY_OK=1,
    NBA97_RECOVERY_ARGUMENT=0,
    NBA97_RECOVERY_OUTSIDE_STORAGE=-1,
    NBA97_RECOVERY_CALLBACK_REQUIRED=-2,
    NBA97_RECOVERY_CALLBACK_FAILED=-3
} Nba97GameRecoveryResult;

/* Exact649D8 call boundary, not an announcement or a queued event. Callback
 * must execute the actual substitution and its transitive effects before
 * returning1;65070 immediately rereads live preferred/inverse/status fields.
 * side is the owned team0/1, active_slot is0..4, bench_slot is the signed
 * source inverse entry (>4), reason is0 and first is1 for the first call in
 * each65070 invocation and0 thereafter. Return0 if that boundary cannot finish.
 * Mutations of exposed state are visible to subsequent source reads. Do not
 * replace the callback with a guessed lineup swap or discard its other effects.
 */
typedef int (*Nba97GameRecoverySubstitute)(void* context,
    Nba97GameLineupRecoveryState* state, unsigned side,
    int32_t active_slot, int32_t bench_slot, int32_t reason, uint32_t first);

/* Complete65070 direct owner (52 instructions), with649D8 as synchronous
 * external boundary. roster_base is the source a1 signed word, ordinarily0/12.
 * Source sets lock1 at entry and clears it only after all five visits succeed.
 */
Nba97GameRecoveryResult nba97_game_lineup_auto_substitute(
    Nba97GameLineupRecoveryState* state, unsigned side, int32_t roster_base,
    Nba97GameRecoverySubstitute substitute, void* context);

/* Complete65140 direct owner (122 instructions), composing native65070.
 * All32-bit elapsed values retain source wrapping23*elapsed and the low16
 * sign-bit saturation quirk. >=120 also performs the source recovery/reorder
 * and CPU/automatic-substitution paths. No gameplay/period-completion claim.
 *
 * These functions mutate live owned state in source order. Null/invalid side
 * arguments have no effects. A reached out-of-storage read or unavailable/
 * failed callback returns explicitly WITH earlier effects retained, including
 * lock1 at an unfinished65070 boundary. Such a return is not source success;
 * do not continue the caller, reset the lock or blindly retry from the start.
 * Use an owned transaction at the integration layer if publication must be
 * atomic. References are guarded only when read. No source out-of-array value
 * is silently clamped, repaired or converted into a replacement player.
 */
Nba97GameRecoveryResult nba97_game_lineup_recover(
    Nba97GameLineupRecoveryState* state, int32_t elapsed,
    Nba97GameRecoverySubstitute substitute, void* context);

#ifdef __cplusplus
}
#endif
#endif
