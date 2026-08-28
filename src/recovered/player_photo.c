#include "player_photo.h"

void nba97_player_photo_reset(Nba97PlayerPhoto* s) {
    /* Layout 0x24 at 80097A24: city slot16 disabled, wait slot18 enabled.
       800310D8 disables photo slot17 when requesting the initial record. */
    s->record = -1;
    s->photo_enabled = s->city_enabled = s->pending = 0;
}

int nba97_player_photo_request(Nba97PlayerPhoto* s, int32_t record) {
    /* 800310D8: identical record returns without hiding/reloading. Only the
       photo object is disabled; an already-enabled city strip remains. */
    if (record < 0 || record == s->record) return 0;
    s->record = record;
    s->photo_enabled = 0;
    s->pending = 1;
    return 1;
}

void nba97_player_photo_complete(Nba97PlayerPhoto* s, int valid) {
    if (!s->pending) return;
    s->pending = 0;
    /* 80030EE4 rejects a bad checksum. 80031084..800310B0 enables the photo
       and preceding city object in state0x24 only after successful decoding.
       The wait object is not toggled: the opaque photograph covers it. */
    if (valid) s->photo_enabled = s->city_enabled = 1;
}
