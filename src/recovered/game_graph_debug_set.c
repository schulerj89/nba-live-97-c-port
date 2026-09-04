#include "game_graph_debug_set.h"

#include <string.h>

#define DEBUG_TEXT UINT32_C(0x80028250)
#define DEBUG_CALLBACK_ADDRESS UINT32_C(0x800c55bc)
#define GRAPH_TYPE_ADDRESS UINT32_C(0x800c55c0)
#define DEBUG_LEVEL_ADDRESS UINT32_C(0x800c55c2)
#define GRAPH_REVERSE_ADDRESS UINT32_C(0x800c55c3)

typedef struct Nba97GameGraphDebugSetRun {
    Nba97GameGraphDebugSetContext* context;
    Nba97GameGraphDebugSetProgress* out;
    uint32_t sp;
    Nba97GameGraphDebugSetValue previous;
} Nba97GameGraphDebugSetRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameGraphDebugSetRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameGraphDebugSetRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameGraphDebugSetRun* run, uint32_t address,
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

static int read_value(Nba97GameGraphDebugSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    Nba97GameGraphDebugSetValue* value) {
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

static int require_known(Nba97GameGraphDebugSetRun* run, uint32_t pc,
    uint32_t address, const Nba97GameGraphDebugSetValue* value) {
    if (value->known)
        return NBA97_TEXT_COMPLETE;
    stop(run, pc, address, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int read_required(Nba97GameGraphDebugSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t* result) {
    Nba97GameGraphDebugSetValue value;
    TRY(read_value(run, address, width, alignment, pc, &value));
    TRY(require_known(run, pc, address, &value));
    *result = value.word;
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameGraphDebugSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameGraphDebugSetRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_value(run, address, 4, 4, pc, value);
}

static int diagnostic(Nba97GameGraphDebugSetRun* run, uint32_t entry,
    uint32_t level, uint32_t type, uint32_t reverse) {
    Nba97GameGraphDebugSetEvent event;
    int result;
    stop(run, 0x80099310u, 0, entry);
    TRY(spend(run));
    /* The source executes jalr directly. Preserve its alignment exception but
     * do not invent a null, executable-range, or callback-table guard. */
    if (entry & 3u)
        return NBA97_TEXT_ALIGNMENT_TRAP;
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x80099310u;
    event.entry = entry;
    event.argument[0] = DEBUG_TEXT;
    event.argument[1] = level;
    event.argument[2] = type;
    event.argument[3] = reverse;
    event.stack_pointer = run->sp;
    event.return_address = 0x80099318u;
    event.saved_register_s0 = run->previous.word;
    event.saved_register_s0_known = run->previous.known;
    event.argument_count = 4;
    result = run->context->io(run->context->user,
        &run->context->memory, &event);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameGraphDebugSetContext* context,
    Nba97GameGraphDebugSetProgress* out,
    Nba97GameGraphDebugSetRun* run) {
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
    out->requested_level = context->level;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_graph_debug_set(Nba97GameGraphDebugSetContext* context,
    Nba97GameGraphDebugSetProgress* out) {
    Nba97GameGraphDebugSetRun storage;
    Nba97GameGraphDebugSetRun* run = &storage;
    uint32_t pointer;
    uint32_t level;
    uint32_t type;
    uint32_t reverse;
    TRY(validate(context, out, run));

    TRY(write_word(run, run->sp + 0x14u, 0x800992d0u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x10u, 0x800992d4u,
        context->saved_register_s0));
    TRY(read_value(run, DEBUG_LEVEL_ADDRESS, 1, 1, 0x800992d8u,
        &run->previous));
    out->previous_level = (uint8_t)run->previous.word;
    out->previous_level_known = run->previous.known;

    /* sb at 0x800992DC and andi at 0x800992E0 make every caller value alias
     * through its low byte. In particular, 0x100 follows the zero path. */
    out->published_level = (uint8_t)context->level;
    TRY(write_value(run, DEBUG_LEVEL_ADDRESS, 1, 1, 0x800992dcu,
        out->published_level));
    if (out->published_level != 0) {
        TRY(read_required(run, DEBUG_CALLBACK_ADDRESS, 4, 4,
            0x800992f0u, &pointer));
        out->diagnostic_callback = pointer;
        TRY(read_required(run, DEBUG_LEVEL_ADDRESS, 1, 1,
            0x800992f4u, &level));
        TRY(read_required(run, GRAPH_TYPE_ADDRESS, 1, 1,
            0x800992fcu, &type));
        TRY(read_required(run, GRAPH_REVERSE_ADDRESS, 1, 1,
            0x80099304u, &reverse));
        out->graph_type = (uint8_t)type;
        out->graph_reverse = (uint8_t)reverse;
        out->diagnostic_called = 1;
        TRY(diagnostic(run, pointer, level, type, reverse));
    }

    /* Both branches assign v0 from the pre-store s0. The child return value
     * is ignored. The epilogue still reloads its two saved words live. */
    out->return_v0 = run->previous.word;
    out->return_v0_known = run->previous.known;
    TRY(read_required(run, run->sp + 0x14u, 4, 4, 0x8009931cu,
        &out->restored_return_address));
    TRY(read_required(run, run->sp + 0x10u, 4, 4, 0x80099320u,
        &out->restored_saved_register_s0));
    out->stack_pointer = run->sp + 0x18u;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
