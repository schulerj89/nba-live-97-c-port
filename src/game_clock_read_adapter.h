#ifndef NBA97_GAME_CLOCK_READ_ADAPTER_H
#define NBA97_GAME_CLOCK_READ_ADAPTER_H

#include "recovered/game_clock_read.h"
#include "recovered/game_speech_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameClockReadAdapterContext {
    size_t operation_budget;
    Nba97GameClockReadAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameClockReadAdapterContext;

typedef struct Nba97GameClockReadAdapterProgress {
    size_t invocations;
    size_t initial_invocations;
    size_t poll_invocations;
    size_t unresolved_callbacks_completed;
    size_t clock_access_events;
    int clock_result;
    Nba97GameSpeechStartupEvent initial_event;
    Nba97GameSpeechStartupEvent poll_event;
    Nba97GameClockReadProgress initial_clock;
    Nba97GameClockReadProgress poll_clock;
} Nba97GameClockReadAdapterProgress;

/* Execute either source-proven speech-startup call to 0x800A5810. The parent
 * exposes only GPRs, so the leaf receives unknown internal HI/LO and returns
 * only its GPR effects; no parent HI/LO value is guessed. */
int nba97_game_clock_read_from_speech_startup(
    const Nba97GameTextMemory*, const Nba97GameSpeechStartupEvent*,
    Nba97GameSpeechStartupRegisters*,
    const Nba97GameClockReadAdapterContext*, Nba97GameClockReadProgress*);

/* Run the actual recovered 0x800800F8 parent with both its initial 0x800801EC
 * and repeated 0x80080208 clock boundaries composed through the leaf. Other
 * speech services remain the caller's explicit typed callback fixtures. */
int nba97_game_speech_startup_with_clock_read(
    const Nba97GameSpeechStartupContext*,
    const Nba97GameClockReadAdapterContext*,
    Nba97GameSpeechStartupProgress*, Nba97GameClockReadAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
