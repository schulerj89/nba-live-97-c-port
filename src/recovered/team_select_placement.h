#ifndef NBA97_TEAM_SELECT_PLACEMENT_H
#define NBA97_TEAM_SELECT_PLACEMENT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* State3's bounded relative-motion projection (2B1E4/2B5AC), not a text pool
 * or GPU primitive record. x/y are the current anchor; x/y minus accumulated
 * offset gives the unchanged source base, with signed16 wrapping. Inactive
 * counters are canonical native values, not inferred allocator RAM bytes. */
typedef struct Nba97TeamPlacementNode {
    int16_t x, y, offset_x, offset_y, dx, dy;
    uint8_t elapsed, duration, relative, alive;
} Nba97TeamPlacementNode;

typedef struct Nba97TeamSelectPlacement {
    Nba97TeamPlacementNode label[12], value[12], arrow[4];
    uint16_t arrow_group; /* All four join120+entry page; Cross does not regroup. */
    uint8_t graphic_count; /* Source3D930 counts the two type41 logo descriptors. */
} Nba97TeamSelectPlacement;

/* Guarded mutations reject null/side>1 without changing the destination.
 * open models completed construction plus queued4FA3C commands, BEFORE the
 * first text presentation. Names0/6 are recreated at final coordinates;
 * away rank values still await their single(-276,-96) shift. */
int nba97_team_select_placement_open(Nba97TeamSelectPlacement*, unsigned side);
/* 39BA8 and4F7B8 use the OLD page. Commands replace any pending command. */
int nba97_team_select_placement_switch_side(Nba97TeamSelectPlacement*, unsigned old_side);
/* 4EF40 recreates the active six VALUE nodes at saved descriptor coordinates.
 * 2C244 mode2 preserves color state only; relative offsets/commands are dropped.
 * Labels and arrows survive. Tint ownership is separate from this module. */
int nba97_team_select_placement_refresh_values(Nba97TeamSelectPlacement*, unsigned side);
/* Call once before composing EACH requested39574 text presentation, including
 * caller waits and Help. Duration1 applies once; the next tick clears relative
 * state without adding another delta. No elapsed-time or catch-up conversion. */
void nba97_team_select_placement_tick(Nba97TeamSelectPlacement*);
/* 2C668 checks only the selected group HEAD, which is its value node because
 * creation prepends the value after its label. Returns0/8; invalid IDs return0.
 * Actual state3 does NOT wait on this result: 3AE4C bypasses the query when
 * graphic_count!=0 (state3 has2). Keep this query separate from poll routing. */
int nba97_team_select_placement_selected_moving(const Nba97TeamSelectPlacement*, unsigned descriptor);

#ifdef __cplusplus
}
#endif
#endif
