#ifndef NBA97_TEAM_SELECT_H
#define NBA97_TEAM_SELECT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_SELECT_TEAMS = 31, NBA97_SELECT_RANKS = 5 };
/* Selection IDs 29/30 are special teams, NOT the roster editor's free agents. */
typedef struct Nba97TeamRanks { uint8_t value[5][31]; } Nba97TeamRanks;
typedef struct Nba97TeamSelect {
    int16_t team[2];                 /* 80021D74/78; 0 right/home, 1 left/away */
    int16_t remembered_regular[2];   /* frontend +70E/+710, updated at exit */
    uint8_t side, criterion;         /* 0 name, 1..5 category rank */
    int8_t result;                   /* 0 running, -1 previous, 1 continue */
    uint8_t sound;
} Nba97TeamSelect;
typedef enum Nba97TeamSelectEvent {
    NBA97_SELECT_NONE, NBA97_SELECT_CRITERION, NBA97_SELECT_TEAM,
    NBA97_SELECT_SIDE, NBA97_SELECT_HELP, NBA97_SELECT_RANDOM,
    NBA97_SELECT_RETURN, NBA97_SELECT_CONTINUE
} Nba97TeamSelectEvent;

/* Constructor guards reject malformed caller data without changing the destination.
 * Each original rank category is a permutation of 1..31 (8004EE7C). */
int nba97_team_ranks_valid(const Nba97TeamRanks*);
int nba97_team_select_open(Nba97TeamSelect*, int home, int away,
                          int remembered_home, int remembered_away);
/* 3D930's remembered descriptor (0..11) survives a screen round trip. */
int nba97_team_select_restore_focus(Nba97TeamSelect*, unsigned descriptor);
/* One selector-dispatched token, not a raw pad bitfield/held-repeat emulator.
 * 3D930 routes Help/Start/Select; descriptor callbacks own all other tokens. */
Nba97TeamSelectEvent nba97_team_select_input(Nba97TeamSelect*,
                                            const Nba97TeamRanks*, uint16_t);
/* 4F934 requests twelve accepted random candidates (0..28). A host supplies
 * the recovered RNG and presentation delays; values 29..31 are rejected. */
int nba97_team_select_random_candidate(Nba97TeamSelect*, uint16_t random_word);
uint32_t nba97_team_select_rng_step(uint32_t state[6]); /* 8007A538 */
typedef struct Nba97TeamRandom { uint8_t remaining, wait; } Nba97TeamRandom;
/* 4F934 applies candidate1 immediately, then waits1..12 presentations.
 * The caller3D930 adds5; 3AE4C pumps1 before its next poll. No cancel inside.
 * Each tick means a completed39574 presentation, not a millisecond or vblank. */
int nba97_team_random_begin(Nba97TeamRandom*, Nba97TeamSelect*, uint32_t rng[6]);
int nba97_team_random_tick(Nba97TeamRandom*, Nba97TeamSelect*, uint32_t rng[6]);
int nba97_team_random_busy(const Nba97TeamRandom*);
#ifdef __cplusplus
}
#endif
#endif
