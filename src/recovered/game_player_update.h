#ifndef NBA97_GAME_PLAYER_UPDATE_H
#define NBA97_GAME_PLAYER_UPDATE_H
#include "game_period.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GamePlayerUpdateField {
    NBA97_UPDATE_A6=0,NBA97_UPDATE_A8,NBA97_UPDATE_A2,NBA97_UPDATE_EA,
    NBA97_UPDATE_1A,NBA97_UPDATE_9A,NBA97_UPDATE_FIELD_COUNT
};
typedef struct Nba97GamePlayerUpdateState {
    Nba97GamePeriodValue entity[11][NBA97_UPDATE_FIELD_COUNT];
    Nba97GamePeriodReference entity_table[11]; /*20BEC; only entries0..9 consumed. */
    Nba97GamePeriodReference current_fdc3c,team_fdc40; /* Team0home,1away. */
    Nba97GamePeriodValue flags_fe8c4;
} Nba97GamePlayerUpdateState;
enum Nba97GamePlayerUpdateOwner {NBA97_UPDATE_CALL_579FC=0,NBA97_UPDATE_CALL_6CFE0};
typedef struct Nba97GamePlayerUpdateCall {
    uint32_t callsite;
    uint8_t owner,slot,entity; /* Captured entity identity, not reread FDC3C. */
} Nba97GamePlayerUpdateCall;
enum Nba97GamePlayerUpdateEventKind {
    NBA97_UPDATE_ENTITY_WRITE=0,NBA97_UPDATE_CURRENT_REFERENCE,NBA97_UPDATE_TEAM_REFERENCE,
    NBA97_UPDATE_FLAGS_WRITE,NBA97_UPDATE_CALLBACK
};
typedef struct Nba97GamePlayerUpdateEvent {
    Nba97GamePeriodValue value;
    Nba97GamePeriodReference reference;
    Nba97GamePlayerUpdateCall call;
    uint8_t kind,entity,field,completed;
} Nba97GamePlayerUpdateEvent;
typedef struct Nba97GamePlayerUpdateReceipt {
    Nba97GamePlayerUpdateEvent event[96]; /* Maximum93 direct stores/calls. */
    uint8_t count,completed;
} Nba97GamePlayerUpdateReceipt;
typedef int (*Nba97GamePlayerUpdateCallback)(void* context,Nba97GamePlayerUpdateState* state,
                                           const Nba97GamePlayerUpdateCall* call);
enum Nba97GamePlayerUpdateResult {
    NBA97_PLAYER_UPDATE_OK=1,NBA97_PLAYER_UPDATE_ARGUMENT=0,
    NBA97_PLAYER_UPDATE_UNRESOLVED=-1,NBA97_PLAYER_UPDATE_REFERENCE=-2,
    NBA97_PLAYER_UPDATE_PENDING=-3,NBA97_PLAYER_UPDATE_CALLBACK_FAILED=-4
};
unsigned nba97_game_player_update_offset(unsigned field);
unsigned nba97_game_player_update_width(unsigned field);
/* Complete6801C. Reads10pointer-table entries in order, preserving aliases and
 * callback edits to later entries. Publishes home/away team refs only at entry
 * and slot5, current entity before each actor; captured entity survives callbacks.
 * Actual579FC then6CFE0 are synchronous boundaries, NOT successful no-ops.
 * Callback returns1complete,0pending,negativefailed; its changes must already be
 * in this view when it returns. Postcallback angle normalization rereads state.
 * Source negative wrap snap retains positive modular delta forEA; actor20 skips
 * EA altogether. Unknown copied/masked values remain unknown, never fakezero.
 * State mutates in source order; failures retain prefix+receipt. Use an outer
 * candidate for atomic publication. State/receipt must not overlap. */
int nba97_game_player_update(Nba97GamePlayerUpdateState* state,
                            Nba97GamePlayerUpdateCallback callback,void* context,
                            Nba97GamePlayerUpdateReceipt* receipt);
#ifdef __cplusplus
}
#endif
#endif
