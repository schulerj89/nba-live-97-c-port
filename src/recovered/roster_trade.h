#ifndef NBA97_ROSTER_TRADE_H
#define NBA97_ROSTER_TRADE_H
#include <stdint.h>
#include "roster_lists.h"
#include "roster_reorder.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97TradeTeams {
    int16_t left, right;
} Nba97TradeTeams;

/* 80056CD0..80056D27: prepare the two team arguments, before 80056494.
 * Uses the shared 80056A94 normalizer, then changes LEFT on a collision.
 * This is a pure entry contract, not a constructed screen or transaction.
 * Signed inputs are preserved, not clamped. The native constructor validates
 * indices before accessing data; 80056128 eligibility is a separate step.
 * No allocation, roster/save writes, assets, or emulated machine state. */
Nba97TradeTeams nba97_trade_prepare_teams(int16_t left, int16_t right,
                                       int16_t mode, int8_t context_team);

enum { NBA97_TRADE_SLOTS=535 };
typedef enum Nba97TradePhase {
    NBA97_TRADE_FIRST, NBA97_TRADE_SECOND, NBA97_TRADE_CLOSED
} Nba97TradePhase;
typedef enum Nba97TradeEvent {
    NBA97_TRADE_IDLE, NBA97_TRADE_ROW, NBA97_TRADE_TEAM, NBA97_TRADE_PICK,
    NBA97_TRADE_SWAPPED, NBA97_TRADE_CANCEL_PICK, NBA97_TRADE_NOTICE,
    NBA97_TRADE_DISCARD_PROMPT, NBA97_TRADE_ACCEPT, NBA97_TRADE_DISCARD,
    NBA97_TRADE_VIEW, NBA97_TRADE_COMPARE, NBA97_TRADE_INVALID
} Nba97TradeEvent;
struct Nba97TradeScreen;
struct Nba97TradeData;
typedef Nba97TradeEvent (*Nba97TradeCallback)(struct Nba97TradeScreen*,
    uint16_t raw, const struct Nba97TradeData*);
typedef struct Nba97TradeScreen {
    /* snapshot is the native durable baseline; undo is the original
       constructor's local480 snapshot, renewed after a child round trip. */
    uint16_t snapshot[535], undo[535], working[535], counts[30];
    int16_t team[2], mode;
    int8_t eligible[16];
    uint8_t cursor[2], top[2], phase, child, waiting, latch;
    uint16_t selected[2], changes, held;
    uint8_t list_kind[2];
    Nba97TradeCallback input_callback[2];
    int16_t selector_result;
    Nba97RosterDecision notice;
    /* Shared 56494 editor: Sign's first list has 100 rows, Trade has15.
       Cursors are native relative slots; list descriptors add the object base. */
    uint8_t frontend_state, selector_action;
    Nba97ReorderTint tint[2][100];
    uint32_t row_revision, presents;
} Nba97TradeScreen;
/* Providers are borrowed per input, never serialized or copied into a save.
 * Native normal frontend supplies mode0; mode2 eligibility remains testable. */
typedef struct Nba97TradeData {
    const uint8_t *positions, *injuries, *preference;
    size_t player_count;
    uint8_t injuries_enabled;
} Nba97TradeData;

int nba97_roster_editor_begin(Nba97TradeScreen*, const uint16_t table[535],
    int16_t left,int16_t right,int16_t mode,const int8_t eligible[16],
    const uint8_t cursor[2],const uint8_t top[2],uint8_t state,
    Nba97TradeCallback first,Nba97TradeCallback second);
unsigned nba97_roster_editor_capacity(const Nba97TradeScreen*,unsigned side);
void nba97_roster_editor_bind(Nba97TradeScreen*);
void nba97_roster_editor_finish_second(Nba97TradeScreen*,int cancelled);
int nba97_roster_editor_providers(const Nba97TradeScreen*,const Nba97TradeData*);
Nba97TradeEvent nba97_roster_editor_child(Nba97TradeScreen*,uint16_t);
Nba97TradeEvent nba97_roster_editor_first(Nba97TradeScreen*,uint16_t,const Nba97TradeData*);

int nba97_trade_begin(Nba97TradeScreen*, const uint16_t table[535],
    int16_t left, int16_t right, int16_t mode, const int8_t eligible[16],
    const uint8_t saved_cursor[2], const uint8_t saved_top[2]);
int nba97_trade_frame(Nba97TradeScreen*, uint16_t raw);
Nba97TradeEvent nba97_trade_input(Nba97TradeScreen*, uint16_t raw, const Nba97TradeData*);
/* 8003D930 selector latch -> 8002F124, with 80055314 cancel override.
 * Call with the accepted controller event, never the key alone: no-ops are
 * silent. Help/notices/confirmation and final accept/discard own their cues. */
uint8_t nba97_trade_event_sound(Nba97TradeEvent, uint16_t raw);
void nba97_trade_dismiss_notice(Nba97TradeScreen*, uint16_t held);
Nba97TradeEvent nba97_trade_discard_answer(Nba97TradeScreen*, int discard, uint16_t held);
int nba97_trade_dirty(const Nba97TradeScreen*);
int nba97_trade_undo_dirty(const Nba97TradeScreen*);
/* 56D50..56D54: preserve the selector's signed halfword result, not a bool.
 * The blocking PSX call becomes an owned state plus frame pump: 0=running,
 * 1=accept, -1=cancel, 2=View, 3=Compare. These are routes, not save outcomes.
 * Callbacks/selector fields are transient and must never enter a save file. */
int32_t nba97_trade_result(const Nba97TradeScreen*);
/* Child return is a proposal until the original confirmation is accepted.
 * Cancel (100) never writes back. Compare additionally requires distinct teams;
 * free-agent children cannot write back. Cursor tops are min(slot,9). */
int nba97_trade_child_proposal(const Nba97TradeScreen*, uint16_t exit_mask,
    const int16_t teams[2], const uint8_t slots[2]);
int nba97_trade_return_child(Nba97TradeScreen*, uint16_t exit_mask,
    const int16_t teams[2], const uint8_t slots[2], int adopt);

#ifdef __cplusplus
}
#endif
#endif
