#ifndef NBA97_TEAM_RATINGS_H
#define NBA97_TEAM_RATINGS_H
#include "team_select.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97TeamRatingInput {
    uint8_t ratings[15][17];
    uint8_t count;
} Nba97TeamRatingInput;
/* 8005DB34 normal-team slice, with 8005D8C8/8005D92C/8005DA78. The host
 * resolves CURRENT roster slots. Original metadata and save identity are
 * never mutated. Private signed team adjustments come from 800A3234. */
int nba97_team_ratings(const Nba97TeamRatingInput teams[29],
                       const int16_t adjustments[29],
                       uint16_t scores[5][29], Nba97TeamRanks* ranks);
#ifdef __cplusplus
}
#endif
#endif
