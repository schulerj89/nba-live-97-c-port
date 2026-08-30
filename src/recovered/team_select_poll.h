#ifndef NBA97_TEAM_SELECT_POLL_H
#define NBA97_TEAM_SELECT_POLL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded state3 physical-pad route in3AE4C/3D930. These are whole normalized
 * 76198 masks, not raw PS1 hardware bits. Queued tokens and state0's complete
 * poll loop are not modeled; only its exact Start history fragment is exposed. */
typedef struct Nba97TeamPadHistory {
    uint16_t prior_mask,repeat_counter; /* shared context+724/+752 */
    uint8_t prior_controller;          /* +71B; empty poll writes255 */
} Nba97TeamPadHistory;
typedef enum Nba97TeamPollPhase {
    NBA97_TEAM_SETTLE, NBA97_TEAM_POLL, NBA97_TEAM_CALLBACK, NBA97_TEAM_POST_WAIT,
    NBA97_TEAM_EXIT_CHANGE, NBA97_TEAM_EXIT_FINAL, NBA97_TEAM_POLL_CLOSED
} Nba97TeamPollPhase;
typedef struct Nba97TeamPoll {
    Nba97TeamPadHistory pad;
    Nba97TeamPollPhase phase;
    uint16_t post_remaining;
} Nba97TeamPoll;
typedef struct Nba97TeamSample {
    uint16_t token,delay;
    uint8_t controller;
} Nba97TeamSample;
enum { NBA97_TEAM_POLL_NONE, NBA97_TEAM_POLL_INPUT, NBA97_TEAM_POLL_EXITED };
/* Reentry resets the loop, never shared pad history. Fresh context bytes are0. */
void nba97_team_poll_open(Nba97TeamPoll*);
/* Request the next source presentation. Only movement flags8/10 queried by
 * 2C668 can block SETTLE; tint flag2 does not. The host supplies that condition. */
int nba97_team_poll_prepare(Nba97TeamPoll*,int selected_text_moving);
/* Exactly once after each requested presentation, including an empty poll.
 * POST/SETTLE never sample raw input; callbacks own their own presentations. */
int nba97_team_poll_presented(Nba97TeamPoll*,const uint16_t pads[8],Nba97TeamSample*);
unsigned nba97_team_poll_caller_wait(uint16_t token,unsigned directional_delay);
void nba97_team_poll_finish_callback(Nba97TeamPoll*,unsigned caller_wait);
/* Both Start and Select:3B194 mandatory presentation/change check, followed
 * by3D930's separate final39574(0,1). No mask/history reset on exit change. */
int nba97_team_poll_exit(Nba97TeamPoll*);
/* State0's exact Start path updates shared history twice before3D930 exits.
 * This is only that source fragment, not state0's full polling/idle behavior. */
int nba97_team_poll_setup_start(Nba97TeamPoll*,unsigned controller);
#ifdef __cplusplus
}
#endif
#endif
