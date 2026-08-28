#ifndef NBA97_COOL_FACT_SELECTION_H
#define NBA97_COOL_FACT_SELECTION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97CoolFacts {
    int8_t flags[5]; /* 593F0: -1 absent, 1 unused; 59E14: 0 used. */
    int8_t selected;
    uint8_t available_mask, draw_mode;
    int8_t upper;
} Nba97CoolFacts;
enum { NBA97_FACT_NONE=0, NBA97_FACT_READY=1, NBA97_FACT_DRAW=2, NBA97_FACT_INVALID=3 };
/* 593F0 / 59D18, split at RNG requests so tests can supply original draws.
 * No PRNG/global-state fidelity is implied by this state machine. */
int nba97_fact_refresh(Nba97CoolFacts*, uint8_t available_mask);
int nba97_fact_offer_random(Nba97CoolFacts*, uint32_t random_value);
int nba97_fact_prepare(Nba97CoolFacts*);
/* 59E14 tail. Host presentation timing remains a separate responsibility. */
int nba97_fact_consume(Nba97CoolFacts*);
/* 59EBC: object21 (2268 / 108) toggled before each of eight presents.
 * Keep this callback in flight until the eighth present has completed. */
typedef struct Nba97FactFlash { uint8_t remaining; } Nba97FactFlash;
int nba97_fact_flash_begin(Nba97FactFlash*);
int nba97_fact_flash_visible(const Nba97FactFlash*);
int nba97_fact_flash_presented(Nba97FactFlash*, Nba97CoolFacts*);
#ifdef __cplusplus
}
#endif
#endif
