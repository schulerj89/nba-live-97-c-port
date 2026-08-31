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

typedef struct Nba97MatchPresentation {
    uint64_t rng_draws,rejected_draws; /* Native receipt; not original fields. */
    uint8_t value,from_schedule;
} Nba97MatchPresentation;
/* FE46D24: mode is the unsigned halfword at frontend context+78.
 * Exactly mode1 first uses schedule_flags&0x60; schedule_flags is the selected
 * schedule record's byte+2, not an invented default or a schedule resolver.
 * Otherwise draw from the existing six-word7A538 stream until draw&0x60!=0.
 * The resulting byte maps to80021DF4; its presentation meaning stays opaque.
 * Required out/rng must be distinct valid objects. Null refusal changes neither.
 * There is no seed, attempt cap, audio event, or16-bit title RNG in this helper.
 * Caller owns transactional publication and support for the selected mode. */
int nba97_match_presentation(Nba97MatchPresentation* out,uint32_t rng[6],
                             uint16_t mode,uint8_t schedule_flags);
#ifdef __cplusplus
}
#endif
#endif
