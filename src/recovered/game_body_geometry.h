#ifndef NBA97_GAME_BODY_GEOMETRY_H
#define NBA97_GAME_BODY_GEOMETRY_H
#include <stddef.h>
#include <stdint.h>
#include "game_period.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Allocation identities are caller-owned registry indices, never PS1 or host
 * addresses. Offset arithmetic is modulo2^32, matching source addu/sll. A
 * reference may be one-past or outside its allocation until actually accessed.
 * Unknown is canonical {0,0,0}; it is not a source NULL pointer. */
typedef struct Nba97GameBodyReference {
    uint32_t allocation,offset;
    uint8_t known;
} Nba97GameBodyReference;
typedef struct Nba97GameBodyCell {
    Nba97GameBodyReference reference;
    uint8_t is_reference; /* 0: rawLE32 bytes, reference metadata must be zero.
                          * 1: explicit native reference, raw bytes are ignored. */
} Nba97GameBodyCell;
typedef struct Nba97GameBodyBuffer {
    uint8_t* bytes;
    uint8_t* known; /* Optional canonical0/1 per raw byte; NULL means all known.
                    * Unknown raw byte payloads need not be fabricated. */
    size_t size;
    Nba97GameBodyCell* cells;
    size_t cell_count;
    /* A reached original aligned word at byte offseto uses cell[(o+mod4)/4].
     * Allocate ceil((size+mod4)/4) cells. This includes a partial leading cell
     * when the original allocation base was not word-aligned. Native pointer
     * alignment alone is never evidence for the original alignment. */
    uint8_t address_mod4,address_mod4_known;
} Nba97GameBodyBuffer;
typedef struct Nba97GameBodyGeometryInput {
    Nba97GameBodyBuffer* buffers;
    size_t buffer_count;
    Nba97GameBodyReference context,cursor; /* Originala0 and a1 (normallybody+8). */
    Nba97GameBodyReference roots_a,roots_b; /* Actual103FD8/10B2B8 owned arrays. */
    Nba97GamePeriodValue count_a_10423c,count_b_fc618,physical_base_febe0;
} Nba97GameBodyGeometryInput;
typedef struct Nba97GameBodyWrite {
    Nba97GameBodyReference destination,reference;
    uint32_t pc,word; /* word is meaningful only for is_reference==0. */
    uint8_t is_reference;
} Nba97GameBodyWrite;
typedef struct Nba97GameBodyGeometryProgress {
    Nba97GameBodyReference cursor,stopped_reference;
    size_t writes;
    uint32_t stopped_pc,return_v0;
    uint8_t players_completed,completed;
} Nba97GameBodyGeometryProgress;
enum Nba97GameBodyGeometryResult {
    NBA97_BODY_OK=1,NBA97_BODY_ARGUMENT=0,NBA97_BODY_UNKNOWN=-1,
    NBA97_BODY_BOUNDS=-2,NBA97_BODY_ALIGNMENT_UNKNOWN=-3,NBA97_BODY_ALIGNMENT_TRAP=-4,
    NBA97_BODY_REFERENCE_REQUIRED=-5,NBA97_BODY_ADDRESS_REQUIRED=-6,
    NBA97_BODY_JOURNAL_LIMIT=-7
};

/* Complete50768, five source contexts atBCC stride. No allocator, GPU tags,
 * numeric source pointer projection, camera,504A8 name-UV tail, or preview
 * decoder. Retained buffer identities and reference cells preserve aliases.
 * Pointer stores set the sidecar and zero their raw bytes as UNUSED metadata;
 * when known[] exists these bytes become unknown. Never feed such bytes to a
 * raw pointer consumer. Scalar stores clear reference metadata and storeLE32.
 * A scalar read of a relocated reference returns ADDRESS_REQUIRED: its actual
 * original allocation base is a separate provenance problem, not a fake base.
 * A reference read never promotes serialized0x80... words into native pointers.
 *
 * All reached LW/SW validate bounds, alignment and whole-span canonical metadata.
 * Counts/index math wraps BEFORE bounds; signed3*count loop tests are literal.
 * BankB corner words use BankA's captured high-byte group. Masking stores and
 * later relocation stores remain distinct and ordered. Roots/parents, first
 * side-player sharedXYZ, both packet banks and descriptor indices are retained.
 * Cursor and sourcev0=0 are separate results. Skipped words are never read or
 * checked for magic constants. No count or malformed index is silently repaired.
 *
 * MUTABLE candidate plus exact prefix journal, not an atomic operation or a
 * resumable cursor. Capacity is checked BEFORE each store, never after losing
 * an event; it also bounds malformed long loops. Caller clones all buffers AND
 * cells and publishes only a complete supported transaction. No successful I/O
 * callbacks exist. Metadata/storage addresses remain fixed during the call.
 * Buffers are canonical nonoverlapping allocations; aliases use the SAME id.
 * Input descriptors, journal, progress, cells and byte/known arrays must not
 * overlap, except the intended reference aliases within one allocation. */
int nba97_game_body_geometry(const Nba97GameBodyGeometryInput*,Nba97GameBodyWrite* journal,
                             size_t capacity,Nba97GameBodyGeometryProgress*);
#ifdef __cplusplus
}
#endif
#endif
