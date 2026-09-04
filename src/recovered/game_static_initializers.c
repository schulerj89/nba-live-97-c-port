#include "game_static_initializers.h"

#include <string.h>

static const uint32_t INITIALIZED_FLAG = 0x800c4b14u;

typedef struct Nba97GameStaticInitializersRun {
    Nba97GameStaticInitializersContext* context;
    Nba97GameStaticInitializersProgress* out;
    uint32_t sp;
} Nba97GameStaticInitializersRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameStaticInitializersRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
}

static int locate(Nba97GameStaticInitializersRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    unsigned j;
    stop(run, pc, address);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
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

static int read_word(Nba97GameStaticInitializersRun* run, uint32_t address,
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

static int write_word(Nba97GameStaticInitializersRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameStaticInitializersContext* context,
    Nba97GameStaticInitializersProgress* out,
    Nba97GameStaticInitializersRun* run) {
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
    run->sp = context->stack_pointer - 0x10u;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_static_initializers(Nba97GameStaticInitializersContext* context,
    Nba97GameStaticInitializersProgress* out) {
    Nba97GameStaticInitializersRun storage;
    Nba97GameStaticInitializersRun* run = &storage;
    TRY(validate(context, out, run));

    /* 0x800948D4 loads the guard before the prologue stores. Its load delay
     * completes during the 0x800948D8 stack adjustment. */
    TRY(read_word(run, INITIALIZED_FLAG, 0x800948d4u,
        &out->initialization_flag));
    TRY(write_word(run, run->sp + 4u, 0x800948dcu,
        context->saved_register[0]));
    TRY(write_word(run, run->sp + 8u, 0x800948e0u,
        context->saved_register[1]));
    TRY(write_word(run, run->sp + 12u, 0x800948e4u,
        context->return_address));
    if (out->initialization_flag) {
        out->already_initialized = 1;
    } else {
        TRY(write_word(run, INITIALIZED_FLAG, 0x800948f4u, 1));
        out->initialized = 1;
        /* 0x80094900..0x8009490C materializes constructor count zero and
         * branches around the otherwise-indirect 0x80094918 call. */
    }

    TRY(read_word(run, run->sp + 12u, 0x80094928u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 8u, 0x8009492cu,
        &out->restored_register[1]));
    TRY(read_word(run, run->sp + 4u, 0x80094930u,
        &out->restored_register[0]));
    out->stack_pointer = run->sp + 0x10u;
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
