#include "game_frame_rate_reset.h"

#include <string.h>

#define CLOCK_ENTRY UINT32_C(0x800a5810)
#define FRAME_COUNTER_OFFSET UINT32_C(0x17c)
#define AUXILIARY_OFFSET UINT32_C(0x180)
#define CLOCK_BASELINE_OFFSET UINT32_C(0x184)
#define INSTANTANEOUS_RATE_OFFSET UINT32_C(0x188)
#define AVERAGE_RATE_OFFSET UINT32_C(0x18c)
#define LAST_REPORT_CLOCK_OFFSET UINT32_C(0x190)

typedef struct Nba97GameFrameRateResetRun {
    Nba97GameFrameRateResetContext* context;
    Nba97GameFrameRateResetProgress* out;
    uint32_t sp;
} Nba97GameFrameRateResetRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameFrameRateResetRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameFrameRateResetRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameFrameRateResetRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    unsigned j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (address & 3u)
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        uint64_t offset = (uint64_t)address - region->base;
        if (address < region->base || offset > region->size ||
            4u > region->size - (size_t)offset)
            continue;
        *data = region->data + (size_t)offset;
        *known = region->known ? region->known + (size_t)offset : 0;
        if (*known)
            for (j = 0; j < 4; ++j)
                if ((*known)[j] > 1)
                    return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}

static int read_word(Nba97GameFrameRateResetRun* run, uint32_t address,
    uint32_t pc, uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    if (known)
        for (i = 0; i < 4; ++i)
            if (!known[i])
                return NBA97_TEXT_UNKNOWN;
    for (i = 0; i < 4; ++i)
        result |= (uint32_t)data[i] << (i * 8u);
    *value = result;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameFrameRateResetRun* run, uint32_t address,
    uint32_t pc, uint32_t value, uint8_t value_known) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    if (value_known > 1)
        return NBA97_TEXT_ARGUMENT;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && !value_known)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = value_known;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameFrameRateResetRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_value(run, address, pc, value, 1);
}

static int read_clock(Nba97GameFrameRateResetRun* run,
    Nba97GameFrameRateResetValue* value) {
    Nba97GameFrameRateResetEvent event;
    int result;
    stop(run, 0x800a7754u, 0, CLOCK_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x800a7754u;
    event.entry = CLOCK_ENTRY;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = 0x800a775cu;
    event.kind = NBA97_GAME_FRAME_RATE_RESET_READ_CLOCK;
    value->word = 0;
    value->known = 0;
    result = run->context->io(run->context->user,
        &run->context->memory, &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameFrameRateResetContext* context,
    Nba97GameFrameRateResetProgress* out,
    Nba97GameFrameRateResetRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < context->memory.count; ++i) {
        const Nba97GameTextRegion* a = &context->memory.region[i];
        if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
            (uint64_t)a->base + a->size > UINT64_C(0x100000000))
            return NBA97_TEXT_ARGUMENT;
        for (j = 0; j < i; ++j) {
            const Nba97GameTextRegion* b = &context->memory.region[j];
            if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
                (uint64_t)b->base < (uint64_t)a->base + a->size)
                return NBA97_TEXT_ARGUMENT;
        }
    }
    run->context = context;
    run->out = out;
    run->sp = context->stack_pointer - 0x18u;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->frame_counter_address = context->global_pointer +
        FRAME_COUNTER_OFFSET;
    out->auxiliary_address = context->global_pointer + AUXILIARY_OFFSET;
    out->clock_baseline_address = context->global_pointer +
        CLOCK_BASELINE_OFFSET;
    out->instantaneous_rate_address = context->global_pointer +
        INSTANTANEOUS_RATE_OFFSET;
    out->average_rate_address = context->global_pointer + AVERAGE_RATE_OFFSET;
    out->last_report_clock_address = context->global_pointer +
        LAST_REPORT_CLOCK_OFFSET;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_frame_rate_reset(Nba97GameFrameRateResetContext* context,
    Nba97GameFrameRateResetProgress* out) {
    Nba97GameFrameRateResetRun storage;
    Nba97GameFrameRateResetRun* run = &storage;
    Nba97GameFrameRateResetValue sample;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800A7738..0x800A7750. Retain the five individual stores
     * and their source order: mapped aliases and child observations make a
     * host memset observably different. The +0x180 word has no reference in
     * the recovered frame-rate consumer, so it remains deliberately unnamed. */
    TRY(write_word(run, run->sp + 0x10u, 0x800a773cu,
        context->return_address));
    TRY(write_word(run, out->frame_counter_address, 0x800a7740u, 0));
    TRY(write_word(run, out->auxiliary_address, 0x800a7744u, 0));
    TRY(write_word(run, out->instantaneous_rate_address, 0x800a7748u, 0));
    TRY(write_word(run, out->average_rate_address, 0x800a774cu, 0));
    TRY(write_word(run, out->last_report_clock_address, 0x800a7750u, 0));

    TRY(read_clock(run, &sample));
    out->sampled_clock = sample.word;
    out->sampled_clock_known = sample.known;
    /* 0x800A775C stores v0 directly and leaves it live as the incidental
     * return value. There is no availability test, clamp, or host-time read. */
    out->return_v0 = sample.word;
    out->return_v0_known = sample.known;
    TRY(write_value(run, out->clock_baseline_address, 0x800a775cu,
        sample.word, sample.known));
    TRY(read_word(run, run->sp + 0x10u, 0x800a7760u,
        &out->restored_return_address));
    run->sp += 0x18u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
