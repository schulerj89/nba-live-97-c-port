#include "game_reset_graph.h"

#include <string.h>

#define DIAGNOSTIC_TEXT UINT32_C(0x80028204)
#define DEBUG_TEXT UINT32_C(0x80028224)
#define DIAGNOSTIC_ENTRY UINT32_C(0x8009cb2c)
#define MEMORY_SET_ENTRY UINT32_C(0x8009bd78)
#define RESET_CALLBACK_ENTRY UINT32_C(0x800985dc)
#define BIOS_A0_49_ENTRY UINT32_C(0x8009bda4)
#define DEVICE_RESET_ENTRY UINT32_C(0x8009b878)
#define DRIVER_TABLE_INITIAL UINT32_C(0x800c5578)
#define DRIVER_TABLE_ADDRESS UINT32_C(0x800c55b8)
#define DEBUG_CALLBACK_ADDRESS UINT32_C(0x800c55bc)
#define GRAPH_STATE UINT32_C(0x800c55c0)
#define WIDTH_TABLE UINT32_C(0x800c5640)
#define HEIGHT_TABLE UINT32_C(0x800c5654)

typedef struct Nba97GameResetGraphRun {
    Nba97GameResetGraphContext* context;
    Nba97GameResetGraphProgress* out;
    uint32_t sp;
    uint32_t s0;
    uint32_t s1;
} Nba97GameResetGraphRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameResetGraphRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameResetGraphRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameResetGraphRun* run, uint32_t address,
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

static int read_value(Nba97GameResetGraphRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    Nba97GameResetGraphValue* value) {
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

static int require_known(Nba97GameResetGraphRun* run, uint32_t pc,
    uint32_t address, const Nba97GameResetGraphValue* value) {
    if (value->known)
        return NBA97_TEXT_COMPLETE;
    stop(run, pc, address, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int write_value(Nba97GameResetGraphRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t value,
    uint8_t value_known) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    if (value_known > 1)
        return NBA97_TEXT_ARGUMENT;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    if (!known && !value_known)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = value_known;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int read_required(Nba97GameResetGraphRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t* result) {
    Nba97GameResetGraphValue value;
    TRY(read_value(run, address, width, alignment, pc, &value));
    TRY(require_known(run, pc, address, &value));
    *result = value.word;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameResetGraphRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_value(run, address, 4, 4, pc, value, 1);
}

static int invoke(Nba97GameResetGraphRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count, uint32_t a0,
    uint32_t a1, uint32_t a2, Nba97GameResetGraphValue* value) {
    Nba97GameResetGraphEvent event;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (kind == NBA97_GAME_RESET_GRAPH_INDIRECT_CALL && (entry & 3u))
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
    result = run->context->io(run->context->user, &run->context->memory,
        &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int direct_call(Nba97GameResetGraphRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count, uint32_t a0, uint32_t a1,
    uint32_t a2, Nba97GameResetGraphValue* value) {
    return invoke(run, pc, entry, NBA97_GAME_RESET_GRAPH_DIRECT_CALL,
        argument_count, a0, a1, a2, value);
}

static int indirect_call(Nba97GameResetGraphRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count, uint32_t a0, uint32_t a1,
    Nba97GameResetGraphValue* value) {
    return invoke(run, pc, entry, NBA97_GAME_RESET_GRAPH_INDIRECT_CALL,
        argument_count, a0, a1, 0, value);
}

static int validate(Nba97GameResetGraphContext* context,
    Nba97GameResetGraphProgress* out, Nba97GameResetGraphRun* run) {
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
    out->requested_mode = context->mode;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_reset_graph(Nba97GameResetGraphContext* context,
    Nba97GameResetGraphProgress* out) {
    Nba97GameResetGraphRun storage;
    Nba97GameResetGraphRun* run = &storage;
    Nba97GameResetGraphValue value;
    Nba97GameResetGraphValue dimension;
    uint32_t index;
    uint32_t pointer;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80099058 prologue. The branch at 0x8009906C has the s0
     * spill in its delay slot, so all three saves precede either path. */
    TRY(write_word(run, run->sp + 0x14u, 0x80099060u, run->s1));
    run->s1 = context->mode & 7u;
    out->masked_mode = (uint8_t)run->s1;
    TRY(write_word(run, run->sp + 0x18u, 0x80099068u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x10u, 0x80099070u, run->s0));

    if (run->s1 == 0 || run->s1 == 3) {
        out->initialized = 1;
        run->s0 = GRAPH_STATE;
        TRY(direct_call(run, 0x80099098u, DIAGNOSTIC_ENTRY, 3,
            DIAGNOSTIC_TEXT, DRIVER_TABLE_INITIAL, GRAPH_STATE, &value));
        TRY(direct_call(run, 0x800990a8u, MEMORY_SET_ENTRY, 3,
            GRAPH_STATE, 0, 0x80u, &value));
        TRY(direct_call(run, 0x800990b0u, RESET_CALLBACK_ENTRY, 0,
            0, 0, 0, &value));

        /* 0x800990C0 reloads this pointer after ResetCallback. BIOS A0:49
         * receives its low 24 bits exactly as in the retail library. */
        TRY(read_required(run, DRIVER_TABLE_ADDRESS, 4, 4,
            0x800990c0u, &pointer));
        out->driver_table = pointer;
        TRY(direct_call(run, 0x800990c8u, BIOS_A0_49_ENTRY, 1,
            pointer & 0x00ffffffu, 0, 0, &value));
        TRY(direct_call(run, 0x800990d0u, DEVICE_RESET_ENTRY, 1,
            run->s1 != 0, 0, 0, &value));
        out->reset_type = (uint8_t)value.word;
        TRY(write_value(run, GRAPH_STATE, 1, 1, 0x800990dcu,
            value.word, value.known));

        TRY(read_required(run, GRAPH_STATE, 1, 1, 0x800990e0u, &index));
        /* Source bug compatibility: the low-level result is truncated to a
         * byte and used as an unchecked index. Do not clamp it to the five
         * retail table entries; malformed children retain the original OOB. */
        TRY(write_value(run, GRAPH_STATE + 1u, 1, 1, 0x800990ecu,
            1, 1));
        TRY(read_value(run, WIDTH_TABLE + index * 4u, 2, 2,
            0x800990fcu, &dimension));
        out->display_width = (uint16_t)dimension.word;
        out->display_width_known = dimension.known;
        TRY(read_required(run, GRAPH_STATE, 1, 1, 0x80099100u, &index));
        TRY(write_value(run, GRAPH_STATE + 4u, 2, 2, 0x80099110u,
            dimension.word, dimension.known));
        TRY(read_value(run, HEIGHT_TABLE + index * 4u, 2, 2,
            0x8009911cu, &dimension));
        out->display_height = (uint16_t)dimension.word;
        out->display_height_known = dimension.known;
        TRY(write_value(run, GRAPH_STATE + 6u, 2, 2, 0x80099124u,
            dimension.word, dimension.known));
        TRY(direct_call(run, 0x80099128u, MEMORY_SET_ENTRY, 3,
            GRAPH_STATE + 0x10u, UINT32_MAX, 0x5cu, &value));
        TRY(direct_call(run, 0x80099138u, MEMORY_SET_ENTRY, 3,
            GRAPH_STATE + 0x6cu, UINT32_MAX, 0x14u, &value));
        TRY(read_value(run, GRAPH_STATE, 1, 1, 0x80099140u, &value));
    } else {
        TRY(read_required(run, GRAPH_STATE + 2u, 1, 1,
            0x80099150u, &pointer));
        if (pointer >= 2u) {
            TRY(read_required(run, DEBUG_CALLBACK_ADDRESS, 4, 4,
                0x80099168u, &pointer));
            out->debug_callback = pointer;
            out->debug_reported = 1;
            /* a1 still carries the caller's unmasked mode at 0x80099174. */
            TRY(indirect_call(run, 0x80099174u, pointer, 2,
                DEBUG_TEXT, context->mode, &value));
        }
        TRY(read_required(run, DRIVER_TABLE_ADDRESS, 4, 4,
            0x80099180u, &pointer));
        out->driver_table = pointer;
        TRY(read_required(run, pointer + 0x34u, 4, 4,
            0x80099188u, &pointer));
        out->driver_reset_target = pointer;
        /* The original performs no null/range guard before this jalr. */
        TRY(indirect_call(run, 0x80099190u, pointer, 1, 1, 0, &value));
    }

    out->return_v0 = value.word;
    out->return_v0_known = value.known;
    TRY(read_required(run, run->sp + 0x18u, 4, 4, 0x80099198u,
        &out->restored_return_address));
    TRY(read_required(run, run->sp + 0x14u, 4, 4, 0x8009919cu,
        &run->s1));
    TRY(read_required(run, run->sp + 0x10u, 4, 4, 0x800991a0u,
        &run->s0));
    out->restored_saved_register[0] = run->s0;
    out->restored_saved_register[1] = run->s1;
    out->stack_pointer = run->sp + 0x20u;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
