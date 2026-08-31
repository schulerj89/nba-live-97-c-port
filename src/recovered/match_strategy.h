#ifndef NBA97_MATCH_STRATEGY_H
#define NBA97_MATCH_STRATEGY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Semantic byte order: team-header +76,+77,+78,+38,+39,+36,+37 (hex).
 * Resident source80021DE6..80021DF3 interleaves home/away for each field;
 * these owned structs instead keep each side together. Meanings stay opaque. */
typedef struct Nba97MatchStrategy {
    uint8_t side[2][7]; /* home, away */
} Nba97MatchStrategy;
typedef struct Nba97MatchTeamStrategy {
    uint8_t fields[7];
} Nba97MatchTeamStrategy;

/* All pointers are required, including on a no-op branch. Return1 on success,
 * 0 on native refusal with output unchanged. Source/destination byte ranges
 * may overlap: helpers capture their inputs before publishing output.
 * These are field projections, not full owners or gameplay initialization. */

/* FE35D80 cold branch, guarded by the full resident32-bit word80021EE4.
 * Zero installs the seven cold values on both sides; nonzero preserves output.
 * The caller owns this flag/lifetime; do not invent a reset at each handoff. */
int nba97_match_strategy_cold(Nba97MatchStrategy* out,uint32_t initialized_word);

/* GAME65820 (full owner116 instructions), no-injury field projection only.
 * side_word is unsigned header+14: zero selects home, ANY nonzero selects away.
 * launch is unsigned8001EDEC; human_count is unsigned header+42.
 * Nonzero launch changes only field0 to1. Ordinary CPU changes only fields0/1
 * to1; an ordinary human-controlled side copies all seven resident bytes.
 * injury_slot<12 refuses before mutation: substitution/count work is not owned.
 * No other team-header bytes, period state or player state are represented. */
int nba97_match_strategy_apply(Nba97MatchTeamStrategy* inout,
    const Nba97MatchStrategy* resident,uint16_t side_word,uint16_t launch,
    uint16_t human_count,uint8_t injury_slot);

/* GAME67930 (full owner76 instructions), its final resident-field copy only.
 * Launch0 copies BOTH teams, including CPU sides; nonzero preserves resident.
 * The caller must reach the source boundary after its prior stats/cleanup work.
 * This helper does not perform stats, cleanup, persistence or source loop exit. */
int nba97_match_strategy_writeback(Nba97MatchStrategy* resident,
    const Nba97MatchTeamStrategy team[2],uint16_t launch);

#ifdef __cplusplus
}
#endif
#endif
