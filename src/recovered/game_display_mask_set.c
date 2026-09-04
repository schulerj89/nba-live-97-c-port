#include "game_display_mask_set.h"

#include <string.h>

#define DEBUG_TEXT UINT32_C(0x800282ac)
#define DEBUG_LEVEL_ADDRESS UINT32_C(0x800c55c2)
#define DEBUG_CALLBACK_ADDRESS UINT32_C(0x800c55bc)
#define DRIVER_TABLE_ADDRESS UINT32_C(0x800c55b8)
#define ENVIRONMENT_CACHE UINT32_C(0x800c562c)
#define MEMORY_SET_ENTRY UINT32_C(0x8009bd78)

typedef struct Nba97GameDisplayMaskSetRun {
    Nba97GameDisplayMaskSetContext* context;
    Nba97GameDisplayMaskSetProgress* out;
    uint32_t sp;
    uint32_t s0;
    uint32_t s1;
} Nba97GameDisplayMaskSetRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameDisplayMaskSetRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameDisplayMaskSetRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameDisplayMaskSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint8_t** data,
    uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (address & (uint32_t)(alignment - 1u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        uint64_t offset = (uint64_t)address - region->base;
        if (address < region->base || offset > region->size ||
            width > region->size - (size_t)offset)
            continue;
        *data = region->data + (size_t)offset;
        *known = region->known ? region->known + (size_t)offset : 0;
        if (*known)
            for (j = 0; j < width; ++j)
                if ((*known)[j] > 1)
                    return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}

static int read_value(Nba97GameDisplayMaskSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    Nba97GameDisplayMaskSetValue* value) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    value->word = 0;
    value->known = 1;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        value->word |= (uint32_t)data[i] << (i * 8u);
        if (known && !known[i])
            value->known = 0;
    }
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int read_required(Nba97GameDisplayMaskSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t* result) {
    Nba97GameDisplayMaskSetValue value;
    TRY(read_value(run, address, width, alignment, pc, &value));
    if (!value.known) {
        stop(run, pc, address, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *result = value.word;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameDisplayMaskSetRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameDisplayMaskSetRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count, uint32_t a0,
    uint32_t a1, uint32_t a2, Nba97GameDisplayMaskSetValue* value) {
    Nba97GameDisplayMaskSetEvent event;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (kind != NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS && (entry & 3u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.argument[0] = a0;
    event.argument[1] = a1;
    event.argument[2] = a2;
    event.stack_pointer = run->sp;
    event.return_address = pc + 8u;
    event.saved_register[0] = run->s0;
    event.saved_register[1] = run->s1;
    event.kind = kind;
    event.argument_count = argument_count;
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

static int validate(Nba97GameDisplayMaskSetContext* context,
    Nba97GameDisplayMaskSetProgress* out,
    Nba97GameDisplayMaskSetRun* run) {
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
    out->requested_mask = context->mask;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_display_mask_set(Nba97GameDisplayMaskSetContext* context,
    Nba97GameDisplayMaskSetProgress* out) {
    Nba97GameDisplayMaskSetRun storage;
    Nba97GameDisplayMaskSetRun* run = &storage;
    Nba97GameDisplayMaskSetValue value;
    uint32_t pointer;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80099458 prologue. The original saves incoming s1 before
     * assigning it the fixed debug-level address, then saves ra and s0. */
    TRY(write_word(run, run->sp + 0x14u, 0x8009945cu, run->s1));
    run->s1 = DEBUG_LEVEL_ADDRESS;
    TRY(write_word(run, run->sp + 0x18u, 0x80099468u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x10u, 0x8009946cu, run->s0));
    TRY(read_required(run, DEBUG_LEVEL_ADDRESS, 1, 1, 0x80099470u,
        &pointer));
    out->debug_level = (uint8_t)pointer;

    /* The move s0,a0 is the 0x8009947C branch delay slot and therefore runs
     * on both paths. No byte truncation or bit mask is applied to the input. */
    run->s0 = context->mask;
    if (out->debug_level >= 2u) {
        TRY(read_required(run, DEBUG_CALLBACK_ADDRESS, 4, 4,
            0x80099490u, &pointer));
        out->debug_callback = pointer;
        out->diagnostic_called = 1;
        TRY(invoke(run, 0x80099498u, pointer,
            NBA97_GAME_DISPLAY_MASK_DIAGNOSTIC, 2,
            DEBUG_TEXT, run->s0, 0, &value));
    }

    /* The branch delay slot always forms 0x800C562C. Only exact zero enters
     * the source memset(-1,20) path; its return is overwritten later. */
    if (run->s0 == 0) {
        out->environment_cache_clear_called = 1;
        TRY(invoke(run, 0x800994acu, MEMORY_SET_ENTRY,
            NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS, 3,
            ENVIRONMENT_CACHE, UINT32_MAX, 0x14u, &value));
    }

    /* Reload the table after both possible callbacks. This ordering lets the
     * debug or clear boundary replace the live table exactly as on PS1. */
    TRY(read_required(run, DRIVER_TABLE_ADDRESS, 4, 4,
        0x800994bcu, &pointer));
    out->driver_table = pointer;

    /* GP1 command 03h is active-low: bit zero means display enabled. The
     * source emits 0x03000001 only for exact zero and 0x03000000 otherwise. */
    out->gpu_control_word = run->s0 == 0 ?
        UINT32_C(0x03000001) : UINT32_C(0x03000000);
    out->display_enabled = (uint8_t)(run->s0 != 0);
    TRY(read_required(run, pointer + 0x10u, 4, 4,
        0x800994ccu, &pointer));
    out->dispatch_target = pointer;
    TRY(invoke(run, 0x800994d4u, pointer,
        NBA97_GAME_DISPLAY_MASK_GPU_CONTROL, 1,
        out->gpu_control_word, 0, 0, &value));
    out->return_v0 = value.word;
    out->return_v0_known = value.known;

    /* The three epilogue loads read mapped bytes after the final child, so a
     * source callback can still alter the restored caller state. */
    TRY(read_required(run, run->sp + 0x18u, 4, 4, 0x800994dcu,
        &out->restored_return_address));
    TRY(read_required(run, run->sp + 0x14u, 4, 4, 0x800994e0u,
        &run->s1));
    TRY(read_required(run, run->sp + 0x10u, 4, 4, 0x800994e4u,
        &run->s0));
    out->restored_saved_register[0] = run->s0;
    out->restored_saved_register[1] = run->s1;
    out->stack_pointer = run->sp + 0x20u;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
