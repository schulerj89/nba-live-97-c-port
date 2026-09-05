#ifndef NBA97_GAME_FRAME_RATE_RESET_H
#define NBA97_GAME_FRAME_RATE_RESET_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameFrameRateResetEventKind {
    NBA97_GAME_FRAME_RATE_RESET_READ_CLOCK = 1
};

typedef struct Nba97GameFrameRateResetValue {
    uint32_t word;
    uint8_t known;
} Nba97GameFrameRateResetValue;

typedef struct Nba97GameFrameRateResetEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameFrameRateResetEvent;

/* GAMEONLY 0x800A5810 remains one explicit synchronous boundary. It returns
 * retained clock word 0x800D7A70; the native port must not substitute host
 * wall-clock cadence. The callback may mutate mapped bytes/knownness,
 * including this owner's live stack frame and the words it already cleared. */
typedef int (*Nba97GameFrameRateResetIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameFrameRateResetEvent*,
    Nba97GameFrameRateResetValue*);

typedef struct Nba97GameFrameRateResetContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t global_pointer;
    Nba97GameFrameRateResetIo io;
    void* user;
} Nba97GameFrameRateResetContext;

typedef struct Nba97GameFrameRateResetProgress {
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
    uint32_t frame_counter_address;
    uint32_t auxiliary_address;
    uint32_t clock_baseline_address;
    uint32_t instantaneous_rate_address;
    uint32_t average_rate_address;
    uint32_t last_report_clock_address;
    uint32_t sampled_clock;
    uint32_t return_v0;
    uint32_t restored_return_address;
    uint8_t sampled_clock_known;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameFrameRateResetProgress;

/* Original GAMEONLY frame-rate tracker reset 0x800A7738..0x800A776F
 * (14 instructions), called by main at 0x80029AD4 immediately before match
 * orchestration. With retail gp=0x800D79C8 it clears 0x800D7B44,
 * 0x800D7B48, 0x800D7B50, 0x800D7B54 and 0x800D7B58, then samples
 * 0x800A5810 into baseline 0x800D7B4C. Consumer 0x800A7460 identifies the
 * state through the original cmn_frate.c / TIMERHZ NOT SET diagnostics.
 *
 * Compatibility deliberately retains source order: all five clears happen
 * before the clock callback, the sampled value is stored without a guard,
 * v0 incidentally remains that sample, and ra is reloaded from the live o32
 * frame. The routine neither establishes host timing nor renders a pixel.
 * Returns NBA97_TEXT_* with exact prefix effects. */
int nba97_game_frame_rate_reset(Nba97GameFrameRateResetContext*,
    Nba97GameFrameRateResetProgress*);

#ifdef __cplusplus
}
#endif
#endif
