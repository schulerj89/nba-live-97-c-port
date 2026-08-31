#ifndef NBA97_GAME_TEAM_ROLES_H
#define NBA97_GAME_TEAM_ROLES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameRolePlayer {
    uint8_t byte0e, byte0f, byte17;
} Nba97GameRolePlayer;
typedef struct Nba97GameRoleEntity {
    uint32_t word00;
    uint16_t opponent_d6, player_reference;
    uint8_t status_reference;
} Nba97GameRoleEntity;
typedef struct Nba97GameTeamRolesInput {
    const Nba97GameRolePlayer* players;
    size_t player_count;
    uint16_t active_player_reference[10]; /* FDC98, independent of entity+20. */
    uint8_t status_byte1e[24]; /* Caller-owned actual status records, not availability+20. */
    uint8_t entity_table[10]; /*20BEC mapping into the physical contiguous entity[] pool. */
    Nba97GameRoleEntity entity[10];
    uint32_t incoming_t6; /* Actual carried word:646A8 supplies8001F984, never guessed0. */
    uint8_t incoming_t6_known;
} Nba97GameTeamRolesInput;

typedef struct Nba97GameTeamRoleTeamEffect {
    uint8_t order5c[5], field61, orderbb[5];
    uint16_t fielda6, fielda8;
} Nba97GameTeamRoleTeamEffect;
enum { NBA97_ROLE_D4=1, NBA97_ROLE_CB=2 };
typedef struct Nba97GameTeamRoleEntityEffect {
    uint16_t fieldd4;
    uint8_t fieldcb, written;
} Nba97GameTeamRoleEntityEffect;
typedef struct Nba97GameTeamRolesEffects {
    Nba97GameTeamRoleTeamEffect team[2];
    Nba97GameTeamRoleEntityEffect entity[10];
    /* Exact relevant register words at each completed helper.64388 preserves
     * t6 but leaves t1 as its last FIRST-pass comparison score.644FC inherits
     * that word; an all-zero byte0F team does not initialize its best-player t1.
     * The second644FC inherits the first644FC's final t1. */
    uint32_t after6459c_t6[2], after6459c_t1[2];
    uint32_t after644fc_t0[2], after644fc_t1[2];
} Nba97GameTeamRolesEffects;

typedef enum Nba97GameTeamRolesResult {
    NBA97_TEAM_ROLES_OK=1, NBA97_TEAM_ROLES_ARGUMENT=0,
    NBA97_TEAM_ROLES_UNKNOWN_REGISTER=-1, NBA97_TEAM_ROLES_PLAYER_REFERENCE=-2,
    NBA97_TEAM_ROLES_ENTITY_REFERENCE=-3, NBA97_TEAM_ROLES_OPPONENT_INDEX=-4,
    NBA97_TEAM_ROLES_STATUS_REFERENCE=-5, NBA97_TEAM_ROLES_PHYSICAL_SPAN=-6
} Nba97GameTeamRolesResult;

/* Complete646A8 tail sequence:6459C(0),6459C(5),644FC(0),644FC(5), including
 * both real64388 calls. Full owners67/40/93 instructions; no callbacks/stubs.
 * The direct646A8 bindings must already have been applied to these inputs.
 * Callers other than646A8 require their actual return-address/stack contract;
 * this API adopts only the proven646A8 call sites and their negative savedRA.
 *
 * Native guards (not retail checks) require valid referenced player/status/
 * entity storage and known incomingt6.644FC walks five physicalF4-stride
 * entities starting at table[side*5], NOT five separately mapped table slots.
 * That span must fit this ten-entity pool. Exact table aliases are supported.
 * Unknown incomingt6 is explicitly refused, never converted to zero. All
 * word00 bits and rating bytes are preserved. Unset entity write bits denote
 * absent effects. Validation failure leaves all output unchanged. Writes are
 * confined to output after reads complete; overlapping input/output is safe.
 */
Nba97GameTeamRolesResult nba97_game_team_roles(
    Nba97GameTeamRolesEffects* out, const Nba97GameTeamRolesInput* input);

#ifdef __cplusplus
}
#endif
#endif
