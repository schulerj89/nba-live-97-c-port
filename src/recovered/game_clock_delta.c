#include "game_clock_delta.h"

#include <string.h>

#define CLOCK_READ_ENTRY UINT32_C(0x800a5810)
#define SNAPSHOT_GP_OFFSET UINT32_C(0x164)

typedef struct Nba97GameClockDeltaRun {
    Nba97GameClockDeltaContext* context;
    Nba97GameClockDeltaProgress* out;
    uint32_t sp;
} Nba97GameClockDeltaRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameClockDeltaRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameClockDeltaRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameClockDeltaRun* run, uint32_t address,
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

static int read_word(Nba97GameClockDeltaRun* run, uint32_t address,
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

static int write_value(Nba97GameClockDeltaRun* run, uint32_t address,
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

static int write_word(Nba97GameClockDeltaRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_value(run, address, pc, value, 1);
}

static int read_clock(Nba97GameClockDeltaRun* run,
    Nba97GameClockDeltaValue* value) {
    Nba97GameClockDeltaEvent event;
    int result;
    stop(run, 0x800a585cu, 0, CLOCK_READ_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x800a585cu;
    event.entry = CLOCK_READ_ENTRY;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = 0x800a5864u;
    event.kind = NBA97_GAME_CLOCK_DELTA_READ_CLOCK;
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

static int validate(Nba97GameClockDeltaContext* context,
    Nba97GameClockDeltaProgress* out, Nba97GameClockDeltaRun* run) {
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
    out->snapshot_address = context->global_pointer + SNAPSHOT_GP_OFFSET;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_clock_delta(Nba97GameClockDeltaContext* context,
    Nba97GameClockDeltaProgress* out) {
    Nba97GameClockDeltaRun storage;
    Nba97GameClockDeltaRun* run = &storage;
    Nba97GameClockDeltaValue sample;
    uint32_t previous;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800A5850..0x800A5858 saves s0, captures the old gp-relative
     * sample, then saves ra. Keep that order because stack/global aliases and
     * child mutations are observable in the original. */
    TRY(write_word(run, run->sp + 0x10u, 0x800a5850u,
        context->saved_register_s0));
    TRY(read_word(run, out->snapshot_address, 0x800a5854u, &previous));
    out->previous_snapshot = previous;
    TRY(write_word(run, run->sp + 0x14u, 0x800a5858u,
        context->return_address));

    TRY(read_clock(run, &sample));
    out->sampled_clock = sample.word;
    out->sampled_clock_known = sample.known;
    TRY(write_value(run, out->snapshot_address, 0x800a5864u,
        sample.word, sample.known));

    /* 0x800A5868 is SUBU: retain modulo-2^32 underflow/wrap behavior. */
    out->return_v0 = sample.word - previous;
    out->return_v0_known = sample.known;
    TRY(read_word(run, run->sp + 0x14u, 0x800a586cu,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x10u, 0x800a5870u,
        &out->restored_saved_register_s0));
    run->sp += 0x18u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
