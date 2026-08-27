#ifndef NBA97_RECOVERED_ROSTER_LISTS_H
#define NBA97_RECOVERED_ROSTER_LISTS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Native descriptors, not emulated addresses. Cursor/top/base are absolute
 * object IDs; count is signed in the original. Capacity bounds host memory. */
typedef struct Nba97RosterList {
    uint16_t *slots;
    uint16_t capacity;
    int16_t team, count, kind;
    uint8_t base, cursor, top;
} Nba97RosterList;

typedef enum Nba97RosterNotice {
    NBA97_ROSTER_NOTICE_NONE, NBA97_ROSTER_NOTICE_INJURED,
    NBA97_ROSTER_NOTICE_MINIMUM, NBA97_ROSTER_NOTICE_EMPTY
} Nba97RosterNotice;
typedef struct Nba97RosterDecision {
    int16_t result; /* Original: 1 allow, 0 reject, -1 destination full. */
    Nba97RosterNotice notice;
    uint32_t message_address;
    int16_t subject; /* Player ID for injury, team ID for minimum, -1 otherwise. */
} Nba97RosterDecision;
typedef struct Nba97RosterValidation {
    int16_t mode, frontend_state;
    uint8_t injuries_enabled;
    const uint16_t *counts; /* 30 entries, including free agents (team29). */
    const uint8_t *injuries;
    size_t player_count;
} Nba97RosterValidation;

/* FUN_800556B0. Message/name lookup is a typed notice consumed by the host;
 * no copied original strings or assets. Invalid native inputs return 0/none. */
Nba97RosterDecision nba97_roster_validate(const Nba97RosterList lists[2],
                                        const Nba97RosterValidation *rules);

/* FUN_800558E0 does NOT validate. Caller must first use validate, and supply
 * the compaction service for transfers. It sees already-updated counts/slots.
 * Return -1: host guard, 0: both empty, 1: mutation/counter increment. */
typedef void (*Nba97RosterCompact)(void *, uint16_t *, int16_t, int16_t);
int nba97_roster_mutate(Nba97RosterList lists[2], uint16_t counts[30],
                       uint16_t *changes, Nba97RosterCompact compact, void *user);

/* FUN_800555F4/5539C dependency contract. Preference data belongs to the local
 * pack/provider, not this source. Use post-transfer counts, as the original
 * does (including its shortened bench-search bound and no-match return0). */
typedef struct Nba97RosterCompaction {
    const uint16_t *counts;
    const uint8_t *positions, *injuries, *preference; /* preference: 5x5 */
    size_t player_count;
    int16_t mode;
    uint8_t injuries_enabled;
} Nba97RosterCompaction;
void nba97_roster_compact(void *context, uint16_t *slots, int16_t team, int16_t removed);

typedef enum Nba97RosterRefreshEvent {
    NBA97_ROSTER_BIND, NBA97_ROSTER_REDRAW, NBA97_ROSTER_PRESENT, NBA97_ROSTER_HEADER
} Nba97RosterRefreshEvent;
typedef void (*Nba97RosterRefreshSink)(void *, Nba97RosterRefreshEvent,
                                     int page, int object, int32_t player);
typedef struct Nba97RosterRefresh {
    Nba97RosterList lists[2];
    uint8_t visible_rows;
    uint16_t descriptor_page;
    int32_t selected[2]; /* Sign-extended original halfwords, including -1. */
    Nba97RosterRefreshSink sink;
    void *user;
} Nba97RosterRefresh;
/* FUN_80055AF8: selector0/1/2; bind all rows, redraw visible only, present one
 * frame BEFORE restoring descriptor page, then refresh headers. Native host
 * presentation is a request; this contract does not emulate GPU/vblank. */
int nba97_roster_refresh_lists(Nba97RosterRefresh *state, int16_t selector);
#ifdef __cplusplus
}
#endif
#endif
