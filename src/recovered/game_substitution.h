#ifndef NBA97_GAME_SUBSTITUTION_H
#define NBA97_GAME_SUBSTITUTION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameSubstitutionTeam {
    uint16_t side14,lineup[12],fielda2,fieldc0,fieldc2;
    uint8_t field34,field77;
} Nba97GameSubstitutionTeam;
typedef struct Nba97GameSubstitutionEntity { uint8_t fieldde,fielddf; } Nba97GameSubstitutionEntity;
typedef struct Nba97GameSubstitutionState {
    Nba97GameSubstitutionTeam team[2];
    uint32_t duration58,remaining60;
    uint16_t saved6c,flag92,marker8e,phase90,flag86,delaya8,lock54;
    uint16_t message8bc,player8c8;
    uint16_t status20[24]; /* Actual status record+20, signed halfword. */
    uint8_t entity_table[10];
    Nba97GameSubstitutionEntity entity[10]; /* Physical entity identity. */
} Nba97GameSubstitutionState;

typedef enum Nba97GameSubstitutionOwner {
    NBA97_SUB_31CB8, NBA97_SUB_29258, NBA97_SUB_64914, NBA97_SUB_64964,
    NBA97_SUB_62BFC, NBA97_SUB_35378, NBA97_SUB_353A0, NBA97_SUB_7F84C,
    NBA97_SUB_7F914, NBA97_SUB_646A8, NBA97_SUB_63EDC, NBA97_SUB_A584C
} Nba97GameSubstitutionOwner;
typedef struct Nba97GameSubstitutionCall {
    Nba97GameSubstitutionOwner owner;
    uint32_t argument[3]; /* Exact source words; signed LH arguments retain all bits. */
    uint8_t argument_count; /* Unused array cells are not source register values. */
} Nba97GameSubstitutionCall;
typedef struct Nba97GameSubstitutionReply {
    uint32_t value;
    uint8_t value_known; /* 31CB8 requires1; only its low byte is consumed. */
} Nba97GameSubstitutionReply;

/* Synchronous boundary: apply the actual callee and its transitive effects,
 * reflecting all changes to this live state before returning1. Other owned
 * data can reside in context. Calls are not notifications or queued audio.
 * Return0 if that owner cannot complete, including a646A8 source divide trap.
 * Only31CB8 needs a known return word; other returns are not consumed.
 * Argument counts describe established semantic inputs, not unknown leftover
 * MIPS registers. No callee implementation is supplied by this interface. */
typedef int (*Nba97GameSubstitutionBoundary)(void* context,
    Nba97GameSubstitutionState* state,const Nba97GameSubstitutionCall* call,
    Nba97GameSubstitutionReply* reply);

typedef enum Nba97GameSubstitutionResult {
    NBA97_SUBSTITUTION_OK=1, NBA97_SUBSTITUTION_ARGUMENT=0,
    NBA97_SUBSTITUTION_OUTSIDE_STORAGE=-1,
    NBA97_SUBSTITUTION_CALLBACK_REQUIRED=-2,
    NBA97_SUBSTITUTION_CALLBACK_FAILED=-3,
    NBA97_SUBSTITUTION_RETURN_UNKNOWN=-4
} Nba97GameSubstitutionResult;

/* GAME649D8 complete249-instruction direct owner. side selects owned header0/1;
 * side14 remains actual header data, not inferred from side. Source args are
 * signed active/bench/reason words and an unmodified first/nonzero selector.
 * Retains wrapping decrements, signed phase/marker branches, signed versus
 * unsigned side arguments, backward bench search and saved-global restoration.
 * All reads after callbacks use current state in exact source order.
 *
 * Native guards, not retail behavior: null/invalid side arguments have no
 * effects. Reached lineup/player/table references outside owned storage, absent
 * callbacks, failed callbacks or unknown query returns stop WITH earlier effects
 * retained. No saved globals are restored on an unfinished boundary. Do not
 * continue the caller or blindly retry. Use an outer owned transaction if
 * publication must be atomic. Never replace an unavailable callback with success
 * or a guessed swap. No period/gameplay-completion claim. */
Nba97GameSubstitutionResult nba97_game_substitute(
    Nba97GameSubstitutionState* state,unsigned side,int32_t active_slot,
    int32_t bench_slot,int32_t reason,uint32_t first,
    Nba97GameSubstitutionBoundary boundary,void* context);

#ifdef __cplusplus
}
#endif
#endif
