#ifndef NBA97_PLAYER_PHOTO_H
#define NBA97_PLAYER_PHOTO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Visibility subset, not the original archive/VRAM object representation. */
typedef struct Nba97PlayerPhoto {
    int32_t record;
    uint8_t photo_enabled, city_enabled, pending;
} Nba97PlayerPhoto;
void nba97_player_photo_reset(Nba97PlayerPhoto* state);
int nba97_player_photo_request(Nba97PlayerPhoto* state, int32_t record);
void nba97_player_photo_complete(Nba97PlayerPhoto* state, int valid);
#ifdef __cplusplus
}
#endif
#endif
