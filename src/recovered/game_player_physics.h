#ifndef NBA97_GAME_PLAYER_PHYSICS_H
#define NBA97_GAME_PLAYER_PHYSICS_H
#include "game_period.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GamePhysicsEntityField {
    NBA97_PHYSICS_00=0,NBA97_PHYSICS_08,NBA97_PHYSICS_0C,NBA97_PHYSICS_10,
    NBA97_PHYSICS_14,NBA97_PHYSICS_16,NBA97_PHYSICS_18,NBA97_PHYSICS_1A,
    NBA97_PHYSICS_24,NBA97_PHYSICS_28,NBA97_PHYSICS_2C,NBA97_PHYSICS_9C,
    NBA97_PHYSICS_A0,NBA97_PHYSICS_A2,NBA97_PHYSICS_C2,NBA97_PHYSICS_C8,
    NBA97_PHYSICS_EE,NBA97_PHYSICS_ENTITY_COUNT
};
enum Nba97GamePhysicsGlobal {
    NBA97_PHYSICS_FDBCC=0,NBA97_PHYSICS_FE8E2,NBA97_PHYSICS_FDB6C,
    NBA97_PHYSICS_FE8E0,NBA97_PHYSICS_FDB90,NBA97_PHYSICS_FE8CC,
    NBA97_PHYSICS_FE8C4,NBA97_PHYSICS_FE8BC,NBA97_PHYSICS_FDBD4,
    NBA97_PHYSICS_FDB58,NBA97_PHYSICS_21D8F,NBA97_PHYSICS_FDB94,
    NBA97_PHYSICS_FE882,NBA97_PHYSICS_FE910,NBA97_PHYSICS_GLOBAL_COUNT
};
typedef struct Nba97GamePlayerPhysicsState {
    Nba97GamePeriodValue entity[NBA97_PHYSICS_ENTITY_COUNT];
    Nba97GamePeriodValue global[NBA97_PHYSICS_GLOBAL_COUNT];
    Nba97GamePeriodValue team_direction10; /* Actual word at current FDC40 target+10. */
} Nba97GamePlayerPhysicsState;
typedef struct Nba97GameDirectionResources {
    const uint8_t* angle_d72b4;
    size_t angle_count; /* Actual owned byte window, indexed from originalD72B4. */
} Nba97GameDirectionResources;
typedef struct Nba97GamePlayerPhysicsResources {
    Nba97GameDirectionResources direction;
    const int8_t* boundary[2]; /* Actual B8A54 and B8A5C signed bytes. */
    size_t boundary_count[2]; /* Source reaches indices0,3,7 only. */
} Nba97GamePlayerPhysicsResources;
typedef struct Nba97GameDirectionEffects {
    Nba97GamePeriodValue angle;
    uint32_t magnitude;
    uint16_t write[2];
    uint8_t count;
} Nba97GameDirectionEffects;
enum Nba97GamePhysicsCallOwner {
    NBA97_PHYSICS_CALL_29590=0,NBA97_PHYSICS_CALL_295C8,
    NBA97_PHYSICS_CALL_62300,NBA97_PHYSICS_CALL_62660
};
typedef struct Nba97GamePhysicsCall {
    uint32_t callsite,argument;
    uint8_t owner,has_argument; /* 62660 has no declared argument; no stale a0 invented. */
} Nba97GamePhysicsCall;
typedef struct Nba97GamePhysicsEvent {
    Nba97GamePeriodValue value;
    Nba97GamePhysicsCall call;
    uint8_t kind,field,completed; /* kind0entity store,1global store,2synchronous call. */
} Nba97GamePhysicsEvent;
typedef struct Nba97GamePhysicsReceipt {
    Nba97GamePhysicsEvent event[40];
    uint8_t count,completed;
} Nba97GamePhysicsReceipt;
typedef int (*Nba97GamePhysicsCallback)(void* context,Nba97GamePlayerPhysicsState* state,
                                      const Nba97GamePhysicsCall* call);
enum Nba97GamePhysicsResult {
    NBA97_PHYSICS_OK=1,NBA97_PHYSICS_ARGUMENT=0,NBA97_PHYSICS_UNRESOLVED=-1,
    NBA97_PHYSICS_REFERENCE=-2,NBA97_PHYSICS_SOURCE_DIVZERO=-3,
    NBA97_PHYSICS_CALLBACK_PENDING=-4,NBA97_PHYSICS_CALLBACK_FAILED=-5
};
unsigned nba97_game_physics_entity_offset(unsigned field);
unsigned nba97_game_physics_entity_width(unsigned field);
unsigned nba97_game_physics_global_address(unsigned field);
unsigned nba97_game_physics_global_width(unsigned field);
/* Full706E4 raw32 helper. Zero vector returns magnitude0 WITHOUT writing angle.
 * NegativeINT_MIN remains negative under source negation and can cause a real
 * divide-by-zero or out-of-window lookup; these are explicit native errors.
 * Atomic effects; previous angle knownness is retained when no store occurs. */
int nba97_game_direction_speed(Nba97GameDirectionEffects* out,uint32_t x,uint32_t y,
                              Nba97GamePeriodValue previous_angle,
                              const Nba97GameDirectionResources* resources);
/* Full6CFE0 plus actual706E4. Mutates a caller-owned candidate in source order.
 * Unlike direction_speed, failure retains the executed prefix and receipt.
 * The caller supplies a transaction/candidate if atomic publication is required.
 * Callbacks run synchronously after prior stores and must publish their actual
 * mutations into this view before returning1;0means pending,-1means failed.
 * No callback means pending at the first reached call, never fictitious success.
 * Rule/audio implementations, input, animation advance and pose are separate.
 * Resources/receipt/state must not overlap. No source tables are embedded. */
int nba97_game_player_physics(Nba97GamePlayerPhysicsState* state,
                             const Nba97GamePlayerPhysicsResources* resources,
                             Nba97GamePhysicsCallback callback,void* context,
                             Nba97GamePhysicsReceipt* receipt);
#ifdef __cplusplus
}
#endif
#endif
