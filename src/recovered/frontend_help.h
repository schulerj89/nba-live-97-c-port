#ifndef NBA97_FRONTEND_HELP_H
#define NBA97_FRONTEND_HELP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97HelpRect { int16_t x, y, width, height; } Nba97HelpRect;
typedef enum Nba97HelpPhase {
    NBA97_HELP_CLOSED, NBA97_HELP_GROWING, NBA97_HELP_WAIT_CHANGE,
    NBA97_HELP_READY, NBA97_HELP_SHRINKING, NBA97_HELP_RETURN_BARRIER
} Nba97HelpPhase;
typedef struct Nba97HelpModal {
    Nba97HelpRect target, rect;
    Nba97HelpPhase phase;
    uint16_t held;
} Nba97HelpModal;
typedef enum Nba97HelpEvent {
    NBA97_HELP_NO_EVENT, NBA97_HELP_OPEN_SOUND, NBA97_HELP_CLOSE_SOUND,
    NBA97_HELP_RETURNED
} Nba97HelpEvent;

/* 40FCC/40A1C style-zero, no-choice Help specialization. 30430/30784/309DC
 * own geometry; 3B194 waits for a CHANGE from the invoking/closing mask,
 * not a timer timeout or a mandatory all-buttons-up condition. The host
 * supplies logical UI ticks and sound playback; no PSX frame emulation. */
Nba97HelpEvent nba97_help_open(Nba97HelpModal*, Nba97HelpRect, uint16_t invoking_mask);
/* Direct40A1C callers retain context+724, including zero, rather than replacing
 * it with the requesting key. Geometry/lifecycle are otherwise shared. */
Nba97HelpEvent nba97_modal_open_prior(Nba97HelpModal*,Nba97HelpRect,uint16_t prior_mask);
Nba97HelpEvent nba97_help_input(Nba97HelpModal*, uint16_t raw_mask);
Nba97HelpEvent nba97_help_tick(Nba97HelpModal*, uint16_t raw_mask);
/* Two-phase presentation adapter: geometry advances before drawing; raw input
 * is sampled afterward. Supply a distinct visible snapshot. Terminal growth
 * shows the full box without text:40A1C creates text after that frame returns.
 * Returns1 only when this frame permits nba97_help_input AFTER presentation.
 * Other owners can retain the combined tick API until their phase is audited. */
int nba97_help_prepare_presentation(Nba97HelpModal* state,Nba97HelpModal* shown);
int nba97_help_visible(const Nba97HelpModal*);
int nba97_help_text_visible(const Nba97HelpModal*);
#ifdef __cplusplus
}
#endif
#endif
