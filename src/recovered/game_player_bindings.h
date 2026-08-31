#ifndef NBA97_GAME_PLAYER_BINDINGS_H
#define NBA97_GAME_PLAYER_BINDINGS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_BINDING_TEAMS=2, NBA97_BINDING_ROSTER=12, NBA97_BINDING_ENTITIES=10 };

typedef struct Nba97GameBindingEntityInput {
    uint32_t binding_index; /* Actual entity word+0; indexes the ten active bindings. */
    uint16_t opponent_slot; /* Actual signed halfword+D6, not inferred from side. */
    uint8_t side_byte;      /* Actual unsigned byte+D9; all256 values are retained. */
} Nba97GameBindingEntityInput;

typedef struct Nba97GamePlayerBindingsInput {
    uint16_t lineup[2][12]; /* Header+16..2D; signed halfwords, including negative bench entries. */
    uint16_t player_reference[2][12]; /* Source20B8C/20BBC alias tables, as indices into player_byte9. */
    const uint8_t* player_byte9; /* Owned actual player-record+9 values; no fallback records. */
    size_t player_count;
    uint8_t entity_table[10]; /* Source20BEC[0..9] -> entity[] index; exact aliases allowed. */
    Nba97GameBindingEntityInput entity[10];
} Nba97GamePlayerBindingsInput;

enum Nba97GameBindingWrite {
    NBA97_BINDING_WORD38=1, NBA97_BINDING_STATUS1C=2, NBA97_BINDING_PLAYER20=4,
    NBA97_BINDING_SCALEC6=8, NBA97_BINDING_INVERSEC8=16, NBA97_BINDING_OPPONENTCC=32
};
typedef struct Nba97GameBindingEntityEffect {
    uint16_t word38, player_reference, scale_c6, inverse_c8, opponent_cc;
    uint8_t status_reference; /*0..23: home slots0..11, away slots12..23, stride22h in source. */
    uint8_t written; /* Unset fields are absent effects, not zero-initialized original fields. */
} Nba97GameBindingEntityEffect;

enum Nba97GameBindingTailOwner { NBA97_BINDING_6459C=0, NBA97_BINDING_644FC=1 };
typedef struct Nba97GameBindingTailCall {
    uint8_t owner, side_word; /* Ordered6459C(0),(5),644FC(0),(5), not executed by this core. */
} Nba97GameBindingTailCall;

typedef struct Nba97GamePlayerBindingsEffects {
    uint16_t player_reference[10]; /* FDC98; active binding indices0..9. */
    uint8_t status_reference[10]; /* FDC70; side*12 + current lineup slot. */
    uint16_t inverse_lineup[2][12]; /* Header+80..97; duplicate positive slots: last index wins. */
    Nba97GameBindingEntityEffect entity[10]; /* Keyed by owned entity identity, not table position. */
    uint8_t visited_entities; /* Source table visits, including an entity that traps. */
    uint8_t trap_table_slot; /*0..9 on source division trap;255 otherwise. */
    uint8_t tail_count;
    uint8_t first_6459c_fallback_byte;
    Nba97GameBindingTailCall tail[4];
} Nba97GamePlayerBindingsEffects;

typedef enum Nba97GamePlayerBindingsResult {
    NBA97_PLAYER_BINDINGS_READY=1,
    NBA97_PLAYER_BINDINGS_DIVIDE_TRAP=2,
    NBA97_PLAYER_BINDINGS_ARGUMENT=3,
    NBA97_PLAYER_BINDINGS_LINEUP=4,
    NBA97_PLAYER_BINDINGS_PLAYER_REFERENCE=5,
    NBA97_PLAYER_BINDINGS_ENTITY_REFERENCE=6,
    NBA97_PLAYER_BINDINGS_ENTITY_INDEX=7,
    NBA97_PLAYER_BINDINGS_OPPONENT_REFERENCE=8
} Nba97GamePlayerBindingsResult;

/* GAME646A8, full original owner155 instructions. Projects direct writes and
 * the four ordered callee requests; READY means the caller must still execute
 * those requests, not that the whole646A8 boundary or period has completed.
 * First6459C inherits t6=8001F984; its all-zero-rating byte fallback is84.
 * Later inherited-register dependencies belong to the actual called helpers.
 *
 * Native guards (not retail checks): nonnull input/output/byte9 storage, actual
 * first-five lineup slots0..11; remaining slots0..11 or any negative halfword;
 * referenced players within byte9 storage; consumed entity/table indices0..9.
 * Source allows out-of-array accesses; the native owner refuses to invent them.
 * Only reached references are validated. Guard failure leaves output unchanged.
 *
 * DIVIDE_TRAP preserves source BREAK1C00 at64894: publishes all binding/inverse
 * maps and the exact prefix of entity effects, including +C6=0 on the trapping
 * entity, but no +C8/opponent+CC write for that visit and no tail calls. Earlier
 * opponent visits may already have written that entity's +CC. This is a
 * source outcome, not a native guard. Do not turn it into success or clamp byte9.
 * Writes are confined to output. Input/output/byte9 overlap is safe because
 * publication follows every consumed read (overlapping output bytes are then
 * replaced). Output members not marked written are absent effects.
 */
Nba97GamePlayerBindingsResult nba97_game_player_bindings(
    Nba97GamePlayerBindingsEffects* out, const Nba97GamePlayerBindingsInput* input);

#ifdef __cplusplus
}
#endif
#endif
