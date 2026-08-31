#ifndef NBA97_GAME_CONTROLLERS_H
#define NBA97_GAME_CONTROLLERS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_GAME_CONTROLLER_COUNT=8, NBA97_GAME_PLAYER_COUNT=10 };
enum { NBA97_GAME_SELECTION_UNKNOWN=0, NBA97_GAME_SELECTION_KNOWN=1 };

/* The raw controller+26 halfword is not necessarily a valid entity index.
 * UNKNOWN is native provenance, not an original zero or -1. Its word must be0.
 * KNOWN preserves every raw halfword, including FFFF and invalid entity IDs. */
typedef struct Nba97GameControllerSelection {
    uint16_t word;
    uint8_t known;
} Nba97GameControllerSelection;

typedef struct Nba97GameControllersInput {
    uint8_t assignment[8]; /* Resident21EA6:1 home,2 away, all others neutral. */
    Nba97GameControllerSelection previous_selected[8];
} Nba97GameControllersInput;

typedef struct Nba97GameControllersEffects {
    int16_t player_claim[10]; /* Entity+4 for players0..9. No ball effect. */
    int16_t marker; /* Source halfword800FDBD0. */
    int16_t team_base[8]; /* Controller+24:0 home,5 away,-1 neutral. */
    Nba97GameControllerSelection selected[8];
    uint8_t selected_written[8]; /* Only neutral slots write source+26. */
    uint8_t controller_binding[8]; /* Table slot i binds owned controller i. */
    uint16_t human_count[2]; /* Home/away header+42. */
} Nba97GameControllersEffects;

/* GAMEONLY80065328..800653E4, complete48-instruction owner. Called after both
 * 655B0 team initializers and before65DB0. Emits semantic effects only; never
 * clears the rest of controller/entity records or changes controls/statistics.
 * Joined slots retain their prior raw word AND provenance, including UNKNOWN.
 * Neutral slots become KNOWN FFFF. No controller/entity pointer is fabricated:
 * controller_binding indexes the caller's owned live records, not PS1 RAM.
 * All256 assignment bytes are accepted; this owner does not enforce a5-human
 * limit. Invalid provenance (known>1 or UNKNOWN with nonzero word), null input
 * or null output returns0 without changing output. All tokens validate even
 * when the neutral branch would replace them. Success returns1. Input/output
 * byte ranges may overlap; input is captured before atomic output publication.
 * This owner does not choose players, initialize periods or enable gameplay. */
int nba97_game_controllers_initialize(Nba97GameControllersEffects* out,
                                     const Nba97GameControllersInput* input);

#ifdef __cplusplus
}
#endif
#endif
