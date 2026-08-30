#ifndef NBA97_MATCH_SETUP_H
#define NBA97_MATCH_SETUP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97MatchRosterIndices {
    uint8_t count,active_count,alias[12];
    uint16_t initial_lineup[12];
} Nba97MatchRosterIndices;
/* GAMEONLY63D58 aliases by source count and sets lineup0..11;
 * 655B0 clamps active count to12.
 * Count<=15 is the native ordinary-roster boundary, not a retail branch. */
int nba97_match_roster_indices(Nba97MatchRosterIndices*,unsigned count);
/* 3E7A8 selects3E698/3E714/3E620. Custom uses the saved backup, not active rules.
 * Invalid native arguments leave output unchanged. */
int nba97_match_effective_rules(uint8_t out[14],unsigned style,const uint8_t custom[14]);
/* Frontend option UI order is not a contiguous resident byte array. */
uint32_t nba97_match_option_address(unsigned index);
#ifdef __cplusplus
}
#endif
#endif
