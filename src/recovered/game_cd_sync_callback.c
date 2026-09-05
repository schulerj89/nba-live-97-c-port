#include "game_cd_sync_callback.h"

#include <string.h>

#define CD_SYNC_CALLBACK_GLOBAL UINT32_C(0x800c57e8)

typedef struct Nba97GameCdSyncCallbackRun {
    Nba97GameCdSyncCallbackContext* context;
    Nba97GameCdSyncCallbackProgress* out;
} Nba97GameCdSyncCallbackRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameCdSyncCallbackRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
}

static int locate_word(Nba97GameCdSyncCallbackRun* run, uint32_t address,
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

static int read_word_retaining_unknown(Nba97GameCdSyncCallbackRun* run,
    uint32_t address, uint32_t pc, uint32_t* value, uint8_t* value_known) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    uint8_t result_known = 1;
    unsigned i;
    TRY(locate_word(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        result |= (uint32_t)data[i] << (i * 8u);
        if (known && !known[i])
            result_known = 0;
    }
    *value = result;
    *value_known = result_known;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameCdSyncCallbackRun* run, uint32_t address,
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

static int validate(Nba97GameCdSyncCallbackContext* context,
    Nba97GameCdSyncCallbackProgress* out,
    Nba97GameCdSyncCallbackRun* run) {
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
    out->callback_global = CD_SYNC_CALLBACK_GLOBAL;
    out->requested_callback = context->replacement_callback;
    /* GAMEONLY 0x8009DBF8 forms 0x800C0000 in v0 before attempting the lw.
     * Retain that exact prefix value when a native mapping/budget guard stops
     * before the old callback word can be loaded. */
    out->return_v0 = UINT32_C(0x800c0000);
    out->return_v0_known = 1;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_cd_sync_callback(Nba97GameCdSyncCallbackContext* context,
    Nba97GameCdSyncCallbackProgress* out) {
    Nba97GameCdSyncCallbackRun storage;
    Nba97GameCdSyncCallbackRun* run = &storage;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8009DBFC/0x8009DC04: capture the old value before the raw
     * new pointer replaces it. Unknown old bytes do not suppress the store. */
    TRY(read_word_retaining_unknown(run, CD_SYNC_CALLBACK_GLOBAL,
        0x8009dbfcu, &out->previous_callback,
        &out->previous_callback_known));
    out->return_v0 = out->previous_callback;
    out->return_v0_known = out->previous_callback_known;
    TRY(write_word(run, CD_SYNC_CALLBACK_GLOBAL, 0x8009dc04u,
        context->replacement_callback));
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
