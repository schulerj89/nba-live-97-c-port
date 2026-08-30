#ifndef NBA97_ROSTER_RESET_H
#define NBA97_ROSTER_RESET_H
#include "frontend_help.h"
#include "roster_reorder.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 58104: states 7/27 compare the inline context table (+124); every other
 * signed frontend state compares the ordinary table (pointer at+1196).
 * The caller borrows both tables; no session/save dirty flag is involved.
 * Null chosen/default tables return false as an extra native safety guard. */
int nba97_reset_table_differs(int16_t frontend_state, const uint16_t *normal,
                             const uint16_t *context, const uint16_t *defaults);
/* 57C48: special-active AND signed kind1 disables the card. This existing
 * normal-frontend convenience API does not claim season-mode integration. */
int nba97_reset_enabled(const uint16_t *working, const uint16_t *defaults,
                        uint8_t special_active, int8_t special_kind);
typedef struct Nba97ResetPrompt {
    Nba97HelpModal modal;
    uint16_t cooldown;
    uint8_t choice; /* 0 restore, 1 cancel */
    uint8_t initial_choice;
    Nba97ReorderTint tint[2];
    uint8_t defer_cross,confirm_pending;
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
/* Audited state5 direct40A1C path: inherited724 and eight presentations BEFORE
 * Cross confirmation. Existing Reset callers retain their current behavior. */
int nba97_reset_open_deferred(Nba97ResetPrompt*,Nba97HelpRect,uint16_t prior,int preference);
#ifdef __cplusplus
}
#endif
#endif
