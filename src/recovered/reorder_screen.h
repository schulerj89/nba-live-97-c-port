#ifndef NBA97_REORDER_SCREEN_H
#define NBA97_REORDER_SCREEN_H
#include "roster_reorder.h"
#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_ROSTER_TABLE_SLOTS = 535, NBA97_REORDER_ROWS = 30 };
typedef struct Nba97ReorderRow {
    uint16_t id, player_id;
    int16_t x, y;
    uint8_t page, type, alignment, up, down, team_scan;
} Nba97ReorderRow;
typedef struct Nba97ReorderScreen {
    Nba97ReorderSession selection;
    uint16_t snapshot[NBA97_ROSTER_TABLE_SLOTS];
    uint16_t working[NBA97_ROSTER_TABLE_SLOTS];
    Nba97ReorderRow rows[NBA97_REORDER_ROWS];
    int16_t team, mode;
    int8_t eligible_teams[16];
    uint8_t layout, visible_rows, list_kind[2], image_object[2];
    uint8_t arrow_x[4], arrow_y[4];
    int16_t heading_x, heading_y;
    uint32_t first_callback, second_callback, entry_callback, exit_callback;
    int16_t result;
} Nba97ReorderScreen;

/* Re-order specialization of 80056AEC/80056494. Caller owns immutable asset
 * loading and the frame pump, replacing the original blocking screen engine.
 * Saved second-list cursor/top use absolute object indices 15..29, as on PSX.
 * NULL saved fields mean a fresh entry. No PSX stack/register/GPU emulation. */
int nba97_reorder_screen_enter(Nba97ReorderScreen *screen,
    const uint16_t table[NBA97_ROSTER_TABLE_SLOTS], int16_t requested_team,
    int16_t mode, const int8_t eligible_teams[16],
    const int16_t saved_cursor[2], const int16_t saved_top[2], uint8_t saved_active);
Nba97ReorderEvent nba97_reorder_screen_input(Nba97ReorderScreen *screen, Nba97ReorderAction action);
int nba97_reorder_screen_scan(Nba97ReorderScreen *screen, int direction);
void nba97_reorder_screen_rebind(Nba97ReorderScreen *screen);
void nba97_reorder_screen_save(const Nba97ReorderScreen *screen,
    int16_t cursor[2], int16_t top[2], uint8_t *active);
int16_t nba97_reorder_screen_result(const Nba97ReorderScreen *screen);
#ifdef __cplusplus
}
#endif
#endif
