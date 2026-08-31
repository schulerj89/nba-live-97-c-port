#ifndef NBA97_GAME_COURT_TEXTURES_H
#define NBA97_GAME_COURT_TEXTURES_H
#include "game_image_upload.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCourtTextureState {
    Nba97GameImageUploadState upload;
    Nba97GameImageReference palette_fed1c;
    uint8_t palette_known;
} Nba97GameCourtTextureState;
typedef struct Nba97GameCourtTextureProgress {
    uint32_t stopped_pc,index;
    int64_t stopped_offset;
    size_t images_completed,palette_stores,palette_width_stores;
    Nba97GameImageUploadProgress image;
    uint8_t completed;
} Nba97GameCourtTextureProgress;
enum { NBA97_COURT_TEXTURE_IMAGE_LIMIT=-100 };

/* Original479B8's487B8..48894 texture loop, plus completeA3FE0/A3FEC lookup.
 * Starts with the already-loaded actual SHPP container, ends before994F4/free.
 * Executes the existing full946B8 image owner and its real transfer callback.
 * This is not whole479B8, file selection/loading, sync, free, or court geometry
 * relocation. Do not claim those callers merely because this loop completes.
 *
 * Count is reread for both the loop and entry lookup. Four-bit palette width
 * uses a SIGNED comparison and is capped at16 before imageXY are reread. The
 * actual palette reference is published first. Eight-bit/direct format paths
 * advance the separate row cursor. All image/header writes and transfers keep
 * source ordering, including aliases and callbacks changing later entries.
 * Unknown bytes, alignment, missing storage and unsupported upload domains
 * refuse. No payload preflight, replacement pixels or invented source addresses.
 * Refusal retains palette/global/image/VRAM effects; clone all owners for atomic
 * publication. This entry is not resumable. Source stack/global-slot aliases
 * into the resource and address-space-wrapping allocation views are excluded.
 * State, progress and metadata must not overlap resource byte/knownness arrays.
 * Metadata/lifetimes remain fixed; callbacks may mutate retained contents and
 * upload state, but not input descriptors or progress. Inputs use the existing
 * original alignment provenance, independent of native pointer alignment.
 */
int nba97_game_court_textures(Nba97GameImageReference,
    Nba97GameCourtTextureState*,size_t image_budget,size_t header_budget,
    Nba97GameImageTransferIo,void*,Nba97GameCourtTextureProgress*);
#ifdef __cplusplus
}
#endif
#endif
