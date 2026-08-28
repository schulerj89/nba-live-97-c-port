#ifndef NBA97_ROSTER_SIGN_H
#define NBA97_ROSTER_SIGN_H
#include "roster_trade.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Sign uses the same owned56494 editor, with its own callbacks and100/15 lists. */
int nba97_sign_begin(Nba97TradeScreen*,const uint16_t table[535],int16_t destination,
    int16_t mode,const int8_t eligible[16],const uint8_t cursor[2],const uint8_t top[2]);
Nba97TradeEvent nba97_sign_first(Nba97TradeScreen*,uint16_t,const Nba97TradeData*);
Nba97TradeEvent nba97_sign_second(Nba97TradeScreen*,uint16_t,const Nba97TradeData*);
/* 57B00 returns vacancy COUNT, not normalized bool. Restriction byte21D96
   is supplied by the caller; no invented season-state meaning. */
int nba97_sign_available(const uint16_t table[535],int16_t mode,uint8_t restriction,
    const int8_t eligible[16]);
#ifdef __cplusplus
}
#endif
#endif
