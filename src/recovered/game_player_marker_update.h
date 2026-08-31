#ifndef NBA97_GAME_PLAYER_MARKER_UPDATE_H
#define NBA97_GAME_PLAYER_MARKER_UPDATE_H
#include "game_player_marker_resources.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete4A044 actor-marker palette/image and banked packet update. Original
 * FAC20 increment/wrap gate, selection, palette colors, texture page, CLUT,
 * and live UV stores are retained. Incoming arguments are unused. Uses the
 * shared marker contract; only actual946B8 and994F4 remain external IO.
 * Upload must compose game_image_upload.c on retained mutable source memory
 * and a real VRAM transfer backend. No palette, packet or state defaults.
 * Private ABI stack/code cannot alias visible inputs. Failed calls retain all
 * preceding source mutations; progress is diagnostic, not a resume cursor.
 */
int nba97_game_player_marker_update(Nba97PlayerMarkerContext*,Nba97PlayerMarkerProgress*);
#ifdef __cplusplus
}
#endif
#endif
