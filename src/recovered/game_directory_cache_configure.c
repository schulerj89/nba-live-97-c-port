#include "game_directory_cache_configure.h"

#include <string.h>

#define CACHE_CAPACITY_ADDRESS UINT32_C(0x800c4ab8)
#define CACHE_POINTER_ADDRESS UINT32_C(0x801046a0)

typedef struct Nba97GameDirectoryCacheConfigureRun {
    Nba97GameDirectoryCacheConfigureContext* context;
    Nba97GameDirectoryCacheConfigureProgress* out;
    uint32_t sp;
    uint32_t fp;
} Nba97GameDirectoryCacheConfigureRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameDirectoryCacheConfigureRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
}

static int locate(Nba97GameDirectoryCacheConfigureRun* run,
    uint32_t address, uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
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

static int write_word(Nba97GameDirectoryCacheConfigureRun* run,
    uint32_t address, uint32_t pc, uint32_t value) {
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

static int read_word(Nba97GameDirectoryCacheConfigureRun* run,
    uint32_t address, uint32_t pc, uint32_t* value) {
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

static int validate(Nba97GameDirectoryCacheConfigureContext* context,
    Nba97GameDirectoryCacheConfigureProgress* out,
    Nba97GameDirectoryCacheConfigureRun* run) {
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
    run->sp = context->stack_pointer - 8u;
    run->fp = context->frame_pointer;
    out->cache_address = context->cache_address;
    out->entry_capacity = context->entry_capacity;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_directory_cache_configure(
    Nba97GameDirectoryCacheConfigureContext* context,
    Nba97GameDirectoryCacheConfigureProgress* out) {
    Nba97GameDirectoryCacheConfigureRun storage;
    Nba97GameDirectoryCacheConfigureRun* run = &storage;
    uint32_t value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80092C7C uses an eight-byte frame and the caller's argument
     * home slots. Reloading those slots preserves source-order alias effects. */
    TRY(write_word(run, run->sp, 0x80092c80u, run->fp));
    run->fp = run->sp;
    TRY(write_word(run, run->fp + 8u, 0x80092c88u,
        context->cache_address));
    TRY(write_word(run, run->fp + 12u, 0x80092c8cu,
        context->entry_capacity));
    TRY(read_word(run, run->fp + 12u, 0x80092c90u, &value));
    TRY(write_word(run, CACHE_CAPACITY_ADDRESS, 0x80092c98u, value));
    out->published_entry_capacity = value;
    TRY(read_word(run, run->fp + 8u, 0x80092c9cu, &value));
    out->return_v0 = value;
    TRY(write_word(run, CACHE_POINTER_ADDRESS, 0x80092ca4u, value));
    out->published_cache_address = value;
    run->sp = run->fp;
    TRY(read_word(run, run->sp, 0x80092cacu, &run->fp));
    run->sp += 8u;

    out->stack_pointer = run->sp;
    out->restored_frame_pointer = run->fp;
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
