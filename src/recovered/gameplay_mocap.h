#ifndef NBA97_GAMEPLAY_MOCAP_H
#define NBA97_GAMEPLAY_MOCAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    NBA97_GAME_MOCAP_CHANNELS = 2,
    NBA97_GAME_MOCAP_SLOTS = 84,
    NBA97_GAME_MOCAP_MAX_HEADERS = 168,
    NBA97_GAME_MOCAP_NONE = 0xffff
};

typedef struct Nba97GameMocapHeader {
    uint32_t header_offset, data_offset;
    uint16_t source_flags, flags;
    uint8_t source_timing, timing, source_count, count;
} Nba97GameMocapHeader;

typedef struct Nba97GameMocapIndex {
    uint32_t directory_offset[NBA97_GAME_MOCAP_CHANNELS];
    uint16_t reference[NBA97_GAME_MOCAP_CHANNELS][NBA97_GAME_MOCAP_SLOTS];
    uint16_t header_count;
    Nba97GameMocapHeader header[NBA97_GAME_MOCAP_MAX_HEADERS];
} Nba97GameMocapIndex;

typedef enum Nba97GameMocapResult {
    NBA97_GAME_MOCAP_OK = 0,
    NBA97_GAME_MOCAP_ARGUMENT,
    NBA97_GAME_MOCAP_FILE_SIZE,
    NBA97_GAME_MOCAP_DIRECTORY,
    NBA97_GAME_MOCAP_HEADER,
    NBA97_GAME_MOCAP_OVERLAP,
    NBA97_GAME_MOCAP_RELOCATED_INPUT,
    NBA97_GAME_MOCAP_DATA_TARGET
} Nba97GameMocapResult;

/* Immutable raw-file projection of GAME640D8 (full source owner:132 instructions).
 * The C++ owner supplies file I/O and retains bytes; this does not execute the
 * source retry allocator, publish PS1 pointers, decode frames or run gameplay.
 * Unique headers follow channel0 then channel1, ascending slots; exact aliases
 * share an index. A zero directory ENTRY is NONE; a zero directory OFFSET is a
 * real directory at file offset0. Counts wrap as source byte stores do.
 *
 * Guards below are native input policies, not recovered retail error branches:
 * input/output must be nonnull and disjoint; size must be8..UINT32_MAX; directory
 * and header offsets must be4-aligned with complete336/12-byte extents; headers
 * must not overlap control words, directories or distinct headers. Read-only
 * directory overlap is allowed. Raw flag20 input is refused (different encoding).
 * Signed header-relative data targets must be within [0,size), without payload
 * stride, alignment or extent assumptions. Backward/shared targets are allowed.
 * Output has fixed capacity168 (one per directory entry), so no caller-supplied
 * variable-capacity buffer is accepted. All output bytes remain unchanged on
 * failure; input is never modified. On success unused headers are zero.
 */
Nba97GameMocapResult nba97_game_mocap_index(const uint8_t* file, size_t file_size,
                                         Nba97GameMocapIndex* out);

#ifdef __cplusplus
}
#endif
#endif
