#ifndef NBA97_GAME_GAMEPLAY_AUDIO_H
#define NBA97_GAME_GAMEPLAY_AUDIO_H

#include "game_player_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Nba97GameplayAudioEntry;
#define NBA97_GAMEPLAY_AUDIO_SOUND_29258 UINT32_C(0x80029258)
#define NBA97_GAMEPLAY_AUDIO_EVENT_29590 UINT32_C(0x80029590)

enum { NBA97_GAMEPLAY_AUDIO_SERVICE_REQUIRED = -14 };

typedef struct Nba97GameplayAudioCall {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[3];
    unsigned count;
    unsigned return_bytes;
} Nba97GameplayAudioCall;

/* AC080 and AB0B8 are the exact source frontiers. The callback must execute
 * synchronously against the same retained GAME/audio state and return a fully
 * known, non-reference 32-bit v0. Reaching this callback is a routed request;
 * it is not by itself evidence that an SPU voice or host device played. */
typedef int (*Nba97GameplayAudioService)(
    void*, const Nba97GameplayAudioCall*, Nba97PlayerFrameValue* result);

typedef struct Nba97GameplayAudioContext {
    Nba97PlayerFrameAccess access;
    Nba97GameplayAudioService service;
    void* user;
    size_t operation_budget;
} Nba97GameplayAudioContext;

typedef struct Nba97GameplayAudioResult {
    uint32_t word;
    uint8_t known;
} Nba97GameplayAudioResult;

typedef struct Nba97GameplayAudioProgress {
    size_t operations;
    size_t reads;
    size_t stores;
    size_t services;
    size_t program_calls;
    size_t scheduler_calls;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint8_t completed;
} Nba97GameplayAudioProgress;

/* Complete GAME 29258 and 29590 request routing, including CPU-only 29200,
 * AB0B8 and the 93D94/93DD4 lock-counter effects. AC080 maps to the existing
 * recovered voice-program owner. The rare 93DD4 pending-drain call to 93734
 * is a typed scheduler boundary; its return is ignored (return_bytes=0).
 * request is the original a0; both public entries apply their source signed
 * 16-bit truncation. Earlier flag writes remain visible if AC080 refuses.
 * No default bank, velocity, sequence record, lock/pending count, program
 * table, voice or SPU state is invented. 295C8 is a separate two-instruction
 * no-op and is not owned here. */
int nba97_game_gameplay_audio(
    Nba97GameplayAudioContext*, Nba97GameplayAudioEntry, uint32_t request,
    Nba97GameplayAudioResult*, Nba97GameplayAudioProgress*);

#ifdef __cplusplus
}
#endif

#endif
