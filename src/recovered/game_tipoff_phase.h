#ifndef NBA97_GAME_TIPOFF_PHASE_H
#define NBA97_GAME_TIPOFF_PHASE_H
#include "game_period.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameTipoffResult {
    NBA97_TIPOFF_OK=1, NBA97_TIPOFF_ARGUMENT=0, NBA97_TIPOFF_UNKNOWN=-1,
    NBA97_TIPOFF_RANGE=-2, NBA97_TIPOFF_ALIGNMENT=-3,
    NBA97_TIPOFF_PENDING=-4, NBA97_TIPOFF_CALLBACK_FAILED=-5
};
/* A reached original data access, NOT a CPU/device emulator. The adapter maps
 * proven original addresses to retained native objects. It must retain aliases,
 * reject absent spans, validate EVERY reached knownness byte (0/1), and perform
 * writes synchronously. No allocation-wide preflight or fabricated zero state.
 * width is1/2/4; known0 has canonical word0. Opaque copies may remain unknown.
 * Return an Nba97GameTipoffResult; a failed store must not perform that store.
 * On writes the adapter must not change the supplied value. Mixed per-byte
 * knownness that cannot be represented by this whole-value view must refuse,
 * never discard known bytes or turn unknown bytes into known zero. Callbacks
 * may change data/aliases; every subsequent access is resolved anew. */
typedef int (*Nba97GameTipoffAccess)(void*,uint32_t pc,uint32_t address,
                                   unsigned width,int write,Nba97GamePeriodValue*);
typedef struct Nba97GameTipoffCall {
    uint32_t pc,owner,argument[2];
    unsigned count;
} Nba97GameTipoffCall;
/* Run the actual named owner against the SAME mutable state. Return1 completed,
 * 0 pending, negative failed. Consumed v0 must be known. Unconsumed return values
 * need no invented value. No success stubs: callbacks retain all callee effects. */
typedef int (*Nba97GameTipoffCallback)(void*,const Nba97GameTipoffCall*,
                                     Nba97GamePeriodValue* return_v0);
typedef struct Nba97GameTipoffContext {
    Nba97GameTipoffAccess access;
    Nba97GameTipoffCallback call;
    void* user;
} Nba97GameTipoffContext;
typedef struct Nba97GameTipoffReceipt {
    uint32_t stopped_pc,stores,calls,return_v0;
    uint8_t completed,return_known,tip_transition;
} Nba97GameTipoffReceipt;

/* Full2D37C. Output is native temporary X,Z,height in that order, corresponding
 * to the three original output pointers. It must not alias context/state.
 * The first two outputs remain published if the third read refuses. Original
 * FED20/FAA04 rows are signed X,height,Z halfwords at stride8, indexed by the
 * entity's FULL word0 (no clamp to10); missing owned rows explicitly refuse. */
int nba97_game_tipoff_hand(Nba97GameTipoffContext*,uint32_t entity,uint32_t hand,
                          Nba97GamePeriodValue out[3],Nba97GameTipoffReceipt*);
/* Full600F0 hand contact and60008 body fallback. Input words retain original
 * wrap/signed rules;60008 narrows height,distance,mode to signed16 first.
 * Original5FC88 is required only on the reached mode0 acceptance route.
 * Native success is separate from original signed return_v0 (-1/0/2/3 etc.). */
int nba97_game_tipoff_hand_contact(Nba97GameTipoffContext*,uint32_t ball,uint32_t entity,
                                  const uint32_t hand_xyz[3],uint32_t mode,
                                  Nba97GameTipoffReceipt*);
int nba97_game_tipoff_body_contact(Nba97GameTipoffContext*,uint32_t ball,uint32_t entity,
                                  uint32_t height,uint32_t distance,uint32_t mode,
                                  Nba97GameTipoffReceipt*);
/* Full601B8/60240, including FDC30 store BEFORE hand-resource reads. hand must
 * be0 or1 to select one of these two actual owners; mode is full original a2. */
int nba97_game_tipoff_contact(Nba97GameTipoffContext*,uint32_t ball,uint32_t entity,
                             unsigned hand,uint32_t mode,Nba97GameTipoffReceipt*);
/* Full5BC34 plus actual2AB70 shared RNG.58610 remains a required callback;
 * argument1 is the ACTUAL selected receiver pointer, not an inferred teammate. */
int nba97_game_tipoff_release(Nba97GameTipoffContext*,uint32_t entity,
                             Nba97GameTipoffReceipt*);
#ifdef __cplusplus
}
#endif
#endif
