#include "game_resource_loader.h"

#include <string.h>

#define LOAD_ATTEMPT_ENTRY UINT32_C(0x800941c8)

typedef struct Nba97GameResourceLoaderRun {
    Nba97GameResourceLoaderContext* context;
    Nba97GameResourceLoaderProgress* out;
    uint32_t sp;
    uint32_t s0;
    uint32_t s1;
} Nba97GameResourceLoaderRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameResourceLoaderRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameResourceLoaderRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate_word(Nba97GameResourceLoaderRun* run, uint32_t address,
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

static int write_word(Nba97GameResourceLoaderRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate_word(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameResourceLoaderRun* run, uint32_t address,
    uint32_t pc, uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    unsigned i;
    TRY(locate_word(run, address, pc, &data, &known));
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

static int validate(Nba97GameResourceLoaderContext* context,
    Nba97GameResourceLoaderProgress* out,
    Nba97GameResourceLoaderRun* run) {
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
    run->sp = context->stack_pointer - 0x20u;
    run->s0 = context->saved_register[0];
    run->s1 = context->saved_register[1];
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->filename = context->filename;
    out->flags = context->flags;
    return NBA97_TEXT_COMPLETE;
}

static int load_attempt(Nba97GameResourceLoaderRun* run,
    Nba97GameResourceLoaderValue* value) {
    Nba97GameResourceLoaderEvent event;
    int result;
    stop(run, 0x80029c18u, 0, LOAD_ATTEMPT_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x80029c18u;
    event.entry = LOAD_ATTEMPT_ENTRY;
    event.argument[0] = run->s0;
    event.argument[1] = run->s1;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.saved_register[0] = run->s0;
    event.saved_register[1] = run->s1;
    event.return_address = 0x80029c20u;
    event.kind = NBA97_GAME_RESOURCE_LOADER_ATTEMPT;
    event.argument_count = 2;
    event.saved_register_known[0] = 1;
    event.saved_register_known[1] = 1;
    value->word = 0;
    value->known = 0;
    result = run->context->io(run->context->user,
        &run->context->memory, &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->load_attempts;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_resource_loader(Nba97GameResourceLoaderContext* context,
    Nba97GameResourceLoaderProgress* out) {
    Nba97GameResourceLoaderRun storage;
    Nba97GameResourceLoaderRun* run = &storage;
    Nba97GameResourceLoaderValue value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80029BFC..0x80029C10 saves incoming s0, s1 and ra in
     * source order while caching filename and flags in s0/s1. */
    TRY(write_word(run, run->sp + 0x10u, 0x80029c00u, run->s0));
    run->s0 = context->filename;
    TRY(write_word(run, run->sp + 0x14u, 0x80029c08u, run->s1));
    run->s1 = context->flags;
    TRY(write_word(run, run->sp + 0x18u, 0x80029c10u,
        context->return_address));

    for (;;) {
        /* 0x80029C18 calls the real attempt. A known-null result branches
         * straight back to the JAL after its delay slot reloads cached a0.
         * Preserve that tight, unbounded retry rather than returning NULL. */
        TRY(load_attempt(run, &value));
        if (!value.known) {
            stop(run, 0x80029c20u, 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (value.word)
            break;
        ++out->null_results;
    }

    out->return_v0 = value.word;
    out->return_v0_known = value.known;
    /* 0x80029C28..0x80029C30 reload mutable stack storage in the exact
     * ra/s1/s0 order; the successful child v0 remains the function result. */
    TRY(read_word(run, run->sp + 0x18u, 0x80029c28u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x14u, 0x80029c2cu, &run->s1));
    TRY(read_word(run, run->sp + 0x10u, 0x80029c30u, &run->s0));
    out->restored_saved_register[0] = run->s0;
    out->restored_saved_register[1] = run->s1;
    run->sp += 0x20u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
