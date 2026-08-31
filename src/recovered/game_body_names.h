#ifndef NBA97_GAME_BODY_NAMES_H
#define NBA97_GAME_BODY_NAMES_H
#include "game_body_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameBodyNamesState {
    Nba97GameBodyBuffer* buffers;
    size_t buffer_count;
    Nba97GameBodyReference contexts_f0ed8; /* Whole ten-player root, not F0ED4. */
    Nba97GameBodyReference name_polygon[10][4]; /* FEBFC + player*10 + index*4. */
    /* FEDF0 + player*10 + index*4. Only the old EVEN word is consumed.
     * known must be0/1; an unknown word's payload is ignored, not source zero.
     * Initial odd words need not be known; they are overwritten before read. */
    Nba97GamePeriodValue name_center[10][4];
} Nba97GameBodyNamesState;

enum Nba97GameBodyNameTarget {
    NBA97_BODY_NAME_POLYGON=0,NBA97_BODY_NAME_CENTER=1,
    NBA97_BODY_NAME_BYTE=2,NBA97_BODY_NAME_MEMORY_REFERENCE=3
};
typedef struct Nba97GameBodyNameWrite {
    Nba97GameBodyReference destination,reference;
    uint32_t pc,word; /* CENTER: full word. BYTE: low8. POLYGON: reference. */
    uint8_t kind,player,index; /* index=2*bank+packet; UV destination is explicit. */
} Nba97GameBodyNameWrite;
typedef struct Nba97GameBodyNamesProgress {
    Nba97GameBodyReference stopped_reference;
    size_t writes;
    uint32_t stopped_pc;
    uint8_t stopped_kind,player,index,banks_completed,players_completed,completed;
} Nba97GameBodyNamesProgress;

/* Exact504A8 tail505A0..50754, stopped before epilogue50758; no return-value,
 * loader/CRC, allocation, GTE/GPU, callback, name texture or full-frame claim.
 * Player0..9, each bank0 then1, against LIVE retained bytes/reference cells.
 * Each pair writes two references, two centers, six UV bytes, in source order.
 * The saved old FIRST center is reused for BOTH packets (original behavior).
 * Midpoints floor unsigned endpoint sums/2; old center arithmetic wraps32 bits.
 *
 * Uses NBA97_BODY_* results from game_body_geometry.h. Failure retains exact
 * supported prefix; not resumable. A complete run emits200 writes. Capacity
 * is checked before each store. Publish only a completed candidate containing
 * byte/known/cell arrays AND both sidecars. Rebuild views after deep copies.
 *
 * Registry identities are0-based and fixed; aliases use the SAME allocation.
 * Word loads require actual original word alignment. Byte accesses impose no
 * CPU alignment requirement, but require known base-mod4 to locate tag cells.
 * A byte read/write inside a tagged pointer word requires unavailable numeric
 * address bits: ADDRESS_REQUIRED (UNKNOWN for an unknown reference). No raw
 * zero pointer metadata is treated as source bytes or a partial pointer value.
 * Bounds/knownness/cell validity are checked only on reached accesses. Storing
 * a polygon reference does not validate its target span; endpoint reads do.
 * Outputs need not initially be known. Canonical metadata is still required
 * at each reached destination. Unknown references use canonical{0,0,0}.
 * State, progress, journal and storage metadata must not overlap. The fixed
 * polygon/center sidecars are disjoint from retained buffer byte allocations;
 * aliases among packet/header/context views within the registry are retained. */
int nba97_game_body_names(Nba97GameBodyNamesState*,Nba97GameBodyNameWrite*,size_t capacity,
                         Nba97GameBodyNamesProgress*);
#ifdef __cplusplus
}
#endif
#endif
