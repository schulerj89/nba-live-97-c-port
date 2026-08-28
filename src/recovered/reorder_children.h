#ifndef NBA97_REORDER_CHILDREN_H
#define NBA97_REORDER_CHILDREN_H
#include "reorder_screen.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Parent remains owned by its editor; children receive only these read-only
 * identities and a separate draft projection. No 535-slot duplicate here. */
typedef struct Nba97ReorderChild {
    uint8_t state, parent_page, waiting_input_change;
    uint16_t held_mask;
    int16_t team;
    uint8_t cursor[2], top[2];
    uint16_t player_id[2];
} Nba97ReorderChild;

Nba97ReorderEvent nba97_reorder_child_begin(Nba97ReorderScreen*, Nba97ReorderChild*, uint16_t mask);
int nba97_reorder_child_input_ready(Nba97ReorderChild*, uint16_t raw_mask);
int nba97_reorder_child_return(Nba97ReorderScreen*, Nba97ReorderChild*, uint16_t exit_mask);
#ifdef __cplusplus
}
#endif
#endif
