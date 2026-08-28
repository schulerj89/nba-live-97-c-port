#ifndef NBA97_ROSTER_RESET_H
#define NBA97_ROSTER_RESET_H
#include "frontend_help.h"
#include "roster_reorder.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 57C48 + 58104. Caller chooses working table (states 7/27 use +124,
 * normal frontend uses +1196). No filesystem/session dirty flag involved. */
int nba97_reset_enabled(const uint16_t *working, const uint16_t *defaults,
                        uint8_t special_active, int8_t special_kind);
typedef struct Nba97ResetPrompt {
    Nba97HelpModal modal;
    uint16_t cooldown;
    uint8_t choice; /* 0 restore, 1 cancel */
    uint8_t initial_choice;
    Nba97ReorderTint tint[2];
} Nba97ResetPrompt;
enum {
    NBA97_RESET_NONE=0, NBA97_RESET_OPEN=1, NBA97_RESET_UP=2,
    NBA97_RESET_DOWN=4, NBA97_RESET_CHOSEN=8, NBA97_RESET_RETURN=16
};
/* 40A1C choice specialization; geometry shared with Help, not dismissal.
 * Defaults policy is 0 => last choice, nonzero => first. Only Cross (800)
 * chooses; Start/Circle do not silently accept or cancel this modal. */
int nba97_reset_open(Nba97ResetPrompt*, Nba97HelpRect, uint16_t held, int preference);
int nba97_reset_input(Nba97ResetPrompt*, uint16_t raw);
int nba97_reset_tick(Nba97ResetPrompt*, uint16_t raw);
#ifdef __cplusplus
}
#endif
#endif
