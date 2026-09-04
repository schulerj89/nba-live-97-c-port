#include "game_global_pointer_save.h"

#include <string.h>

static const uint32_t SAVED_GLOBAL_POINTER = 0x800d6e2cu;

typedef struct Nba97GameGlobalPointerSaveRun {
    Nba97GameGlobalPointerSaveContext* context;
    Nba97GameGlobalPointerSaveProgress* out;
    uint32_t global_pointer;
} Nba97GameGlobalPointerSaveRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameGlobalPointerSaveRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
}

static int locate(Nba97GameGlobalPointerSaveRun* run, uint32_t address,
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

static int write_word(Nba97GameGlobalPointerSaveRun* run, uint32_t address,
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

static int validate(Nba97GameGlobalPointerSaveContext* context,
    Nba97GameGlobalPointerSaveProgress* out,
    Nba97GameGlobalPointerSaveRun* run) {
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
    run->global_pointer = context->global_pointer;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_global_pointer_save(Nba97GameGlobalPointerSaveContext* context,
    Nba97GameGlobalPointerSaveProgress* out) {
    Nba97GameGlobalPointerSaveRun storage;
    Nba97GameGlobalPointerSaveRun* run = &storage;
    TRY(validate(context, out, run));

    /* 0x800A4830/34 forms 0x800D6E2C; 0x800A4838 performs the
     * routine's only source-visible operation. */
    TRY(write_word(run, SAVED_GLOBAL_POINTER, 0x800a4838u,
        run->global_pointer));
    out->stored_global_pointer = run->global_pointer;
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
