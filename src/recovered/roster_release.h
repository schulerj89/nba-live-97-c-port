#ifndef NBA97_ROSTER_RELEASE_H
#define NBA97_ROSTER_RELEASE_H
#include <stdint.h>
#include "roster_trade.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97ReleasePosition {
    uint8_t cursor; /* Relative free-agent slot, not absolute selector ID. */
    uint8_t top;    /* Six visible rows; entry places the vacancy near row4. */
} Nba97ReleasePosition;

/* 8005721C..80057297: first sentinel search and viewport clamp only.
 * Remaining22 wrapper instructions are owned by nba97_release_begin below.
 * A full pool cannot be entered through the original availability gate.
 * Native guards reject it without reading past the100-slot array. */
int nba97_release_prepare_free_agents(const uint16_t free_agents[100],
                                     Nba97ReleasePosition *position);

/* Full17-instruction80057B6C contract. Deliberately checks slot534,
 * not total vacancies or team count. Null is a separate native guard. */
int nba97_release_available(const uint16_t table[535], int16_t mode,
                            uint8_t restriction);

/* 8005721C wrapper: single-stage donor15/free100 construction, with first
 * callback and NULL second callback. Saved receiver selection is recomputed
 * from its first vacancy, not restored from Sign/Trade. Returns construction
 * success; async selector's signed result is nba97_trade_result(screen). */
int nba97_release_begin(Nba97TradeScreen*, const uint16_t table[535],
    int16_t donor,int16_t mode,const int8_t eligible[16],
    uint8_t donor_cursor,uint8_t donor_top);

/*36-instruction56FF4: conditional one-row scroll, nine-frame continuation,
 * then advance the receiver cursor. nba97_trade_frame resumes the wait.*/
int nba97_release_advance(Nba97TradeScreen*);
/*102-instruction57084: single-stage release, original refusal precedence,
 * shared mutation/compaction and child routes.*/
Nba97TradeEvent nba97_release_callback(Nba97TradeScreen*,uint16_t,const Nba97TradeData*);

#ifdef __cplusplus
}
#endif
#endif
