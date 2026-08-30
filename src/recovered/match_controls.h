#ifndef NBA97_MATCH_CONTROLS_H
#define NBA97_MATCH_CONTROLS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { NBA97_MATCH_CONTROLLERS=8, NBA97_MATCH_CONTROL_BYTES=59,
       NBA97_MATCH_STAT_BYTES=36, NBA97_MATCH_PROFILES=20 };
/* Semantic fields only. Original controller record stride is0x78: stats at0,
 * controls at0x3c. The24-byte gap and final byte are not modeled or cleared. */
typedef struct Nba97MatchControls {
    uint8_t stats[8][36];
    uint8_t map[8][59];
} Nba97MatchControls;
typedef struct Nba97ProfileControls {
    uint8_t map[20][59];
    uint8_t valid[20];
} Nba97ProfileControls;
enum { NBA97_CONTROLS_RETAINED=0, NBA97_CONTROLS_DEFAULT=1, NBA97_CONTROLS_PROFILE=2 };
/* 80061674,77 instructions. Always clears36 statistic bytes for all8 slots.
 * Param!=0 forces defaults; otherwise signed negative selectors retain maps,
 * saved valid!=0 copies59 profile bytes, and saved valid==0 uses defaults.
 * Positive selectors outside20 slots are a native error unless force!=0.
 * Validation fails without modifying live/provenance. All input buffers and
 * provenance must be valid, separate objects; no I/O or private bytes here. */
int nba97_match_controls_finalize(Nba97MatchControls* live,const int8_t selectors[8],
        const Nba97ProfileControls*,const uint8_t defaults[59],int force_default,
        uint8_t provenance[8]);
#ifdef __cplusplus
}
#endif
#endif
