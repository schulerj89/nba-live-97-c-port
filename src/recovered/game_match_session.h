#ifndef NBA97_GAME_MATCH_SESSION_H
#define NBA97_GAME_MATCH_SESSION_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameMatchSessionEventKind {
    NBA97_GAME_MATCH_SESSION_CLEAR_RECTANGLE = 1,
    NBA97_GAME_MATCH_SESSION_FRAME_RATE_RESET = 2,
    NBA97_GAME_MATCH_SESSION_SET_DEF_DRAW_ENV = 3,
    NBA97_GAME_MATCH_SESSION_SET_DEF_DISP_ENV = 4,
    NBA97_GAME_MATCH_SESSION_LOCATION_LOOKUP = 5,
    NBA97_GAME_MATCH_SESSION_INITIALIZE = 6,
    NBA97_GAME_MATCH_SESSION_LOAD_SCENE = 7,
    NBA97_GAME_MATCH_SESSION_RUN_LOOP = 8,
    NBA97_GAME_MATCH_SESSION_TEARDOWN = 9,
    NBA97_GAME_MATCH_SESSION_PRESENTATION_WAIT = 10,
    NBA97_GAME_MATCH_SESSION_DRAW_SYNC = 11
};

typedef struct Nba97GameMatchSessionValue {
    uint32_t word;
    uint8_t known;
} Nba97GameMatchSessionValue;

typedef struct Nba97GameMatchSessionEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[5];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t saved_register[3]; /* Current s0, s1 and s2. */
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMatchSessionEvent;

/* Every child remains an explicit synchronous source boundary. A callback
 * returns 1 only when that boundary was carried out and may mutate mapped
 * bytes/knownness, including the live o32 frame and globals re-read later. */
typedef int (*Nba97GameMatchSessionIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameMatchSessionEvent*, Nba97GameMatchSessionValue*);

typedef struct Nba97GameMatchSessionContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[3];
    uint32_t global_pointer;
    Nba97GameMatchSessionIo io;
    void* user;
} Nba97GameMatchSessionContext;

typedef struct Nba97GameMatchSessionProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t restored_return_address;
    uint32_t restored_saved_register[3];
    uint32_t initial_custom_location;
    uint32_t final_custom_location;
    uint32_t initial_team_index;
    uint32_t post_lookup_team_index;
    uint32_t first_restore_team_index;
    uint32_t second_restore_team_index;
    uint32_t cleared_record_address;
    uint32_t replacement_record_address;
    uint32_t first_restore_record_address;
    uint32_t second_restore_record_address;
    uint32_t saved_team_field[2];
    uint8_t saved_team_field_known_mask[2]; /* One bit per source byte. */
    uint32_t replacement_location;
    uint8_t replacement_location_known;
    size_t clear_rectangle_calls;
    size_t frame_rate_reset_calls;
    size_t environment_calls;
    size_t location_lookup_calls;
    size_t session_stage_calls;
    size_t presentation_wait_calls;
    size_t draw_sync_calls;
    size_t direct_control_bytes_written;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t initial_custom_location_active;
    uint8_t final_custom_location_active;
    uint8_t completed;
} Nba97GameMatchSessionProgress;

/* Original GAMEONLY match-session orchestrator 0x8002D8D4..0x8002DB67
 * (165 instructions), reached from main at call PC 0x80029ADC. It prepares
 * draw/display environments, resets frame-rate state, optionally patches the
 * selected team's custom-location fields, runs initialization/scene/loop/
 * teardown children, restores the saved fields, clears the exit surface and
 * performs eleven presentation waits around DrawSync.
 *
 * Compatibility retains the source's independent before/after custom-location
 * tests and repeated live team-index loads. A value becoming enabled late can
 * therefore restore zero-initialized s1/s2, while a value becoming disabled
 * can skip restoration; changing the index can split patch/restore stores
 * across records. No index bound check is added. Children are mandatory
 * boundaries, not successful no-ops. Returns NBA97_TEXT_* with exact prefix
 * effects and live o32 epilogue reloads. */
int nba97_game_match_session(Nba97GameMatchSessionContext*,
    Nba97GameMatchSessionProgress*);

#ifdef __cplusplus
}
#endif
#endif
