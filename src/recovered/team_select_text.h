#ifndef NBA97_TEAM_SELECT_TEXT_H
#define NBA97_TEAM_SELECT_TEXT_H

#include "roster_reorder.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_TEAM_TEXT_CAPACITY=200, NBA97_TEAM_TEXT_GROUPS=255,
       NBA97_TEAM_TEXT_NONE=65535, NBA97_TEAM_TEXT_SEED_BYTES=600 };

/* Each bit 0/1/2 establishes the corresponding R/G/B channel. Canonical
 * bytes with an unset bit are placeholders, never recovered color claims. */
typedef struct Nba97TeamTextKnown {
    uint8_t start, alternate, target, rgb;
} Nba97TeamTextKnown;

typedef struct Nba97TeamTextNode {
    Nba97ReorderTint tint;
    Nba97TeamTextKnown known;
    int16_t lifetime; /* -1 free, 0 retiring, 32767 permanent */
    uint16_t group;
    uint16_t group_previous, group_next, layer_previous, layer_next;
    uint8_t layer;
    /* Help's group retirement detaches the head before the layer pass frees
     * its nodes; ordinary value retirement keeps its group link until then. */
    uint8_t group_linked;
} Nba97TeamTextNode;

typedef struct Nba97TeamTextState {
    Nba97TeamTextNode slots[NBA97_TEAM_TEXT_CAPACITY];
    uint16_t group_head[NBA97_TEAM_TEXT_GROUPS];
    uint16_t layer_head[3], layer_tail[3];
    uint16_t label[12], value[12], arrow[4], header, help[6];
    uint16_t hint; /* Source scan begins at the last chosen slot, inclusively. */
    uint8_t initialized, anchored, opened, focus, help_active;
} Nba97TeamTextState;

typedef struct Nba97TeamTextPaint {
    Nba97ReorderTint tint;
    uint8_t rgb_known, active;
} Nba97TeamTextPaint;

typedef struct Nba97TeamTextView {
    Nba97TeamTextPaint label[12], value[12], arrow[4], header;
    uint8_t anchored;
} Nba97TeamTextView;

/* Explicit all-free source-domain seed. It does not imply that every retail
 * launch has this state. Rejection leaves the destination unchanged. */
int nba97_team_text_seed(Nba97TeamTextState*, const uint8_t* start_rgb,
                         size_t bytes, unsigned hint);
/* Start a canonical unanchored native epoch: unknown inherited RGB, hint0.
 * Its indices are native bookkeeping, not inferred original allocation IDs. */
void nba97_team_text_unknown(Nba97TeamTextState*);
/* Call after leaving the tracked route for an unmodeled producer. Discards
 * allocation predictions and starts a new UNKNOWN epoch, never a known reset. */
void nba97_team_text_invalidate(Nba97TeamTextState*);

/* Source entry cleanup then29 constructions. No implicit seed/reset on entry. */
int nba97_team_text_open(Nba97TeamTextState*, unsigned focus);
/* 4EF40 only: six active values. Random uses this once per accepted candidate. */
int nba97_team_text_refresh(Nba97TeamTextState*, unsigned side);
/* Complete ordinary Left/Right replacement: six values THEN selected again. */
int nba97_team_text_direction(Nba97TeamTextState*, unsigned side, unsigned focus);
int nba97_team_text_focus(Nba97TeamTextState*, unsigned focus);
/* Caller enforces exact Left8/Right4 and nonzero sound latch after callback. */
int nba97_team_text_flash(Nba97TeamTextState*, unsigned arrow);

/* Help creation follows the terminal growth presentation. Dismissal detaches
 * its six group heads and retires the layer2 nodes before the first shrink. */
int nba97_team_text_help_create(Nba97TeamTextState*);
int nba97_team_text_help_retire(Nba97TeamTextState*);
/* After exit's input-change barrier, BEFORE its separate final presentation. */
int nba97_team_text_retire_all(Nba97TeamTextState*);
/* One accepted text presentation: free retiring nodes without a final tint
 * tick; tick remaining nodes in source layer order. Paint/rebuild must not call. */
int nba97_team_text_present(Nba97TeamTextState*);
int nba97_team_text_view(const Nba97TeamTextState*, Nba97TeamTextView*);

/* Allocation mutators are transactional on exhaustion/invalid state. This
 * bounded metadata model excludes arbitrary lifetimes, cropping, glyph-range
 * coloring, GPU geometry and unrelated frontend producers. Existing tint
 * helpers remain the sole numerical color kernel. */
#ifdef __cplusplus
}
#endif
#endif
