#include "game_controller_suspend.h"

#include <string.h>

#define SUSPEND_FLAG_ADDRESS UINT32_C(0x800c4a70)
#define SHUTDOWN_ENTRY UINT32_C(0x80091224)

typedef struct Nba97GameControllerSuspendRun {
    Nba97GameControllerSuspendContext* context;
    Nba97GameControllerSuspendProgress* out;
    uint32_t sp;
} Nba97GameControllerSuspendRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameControllerSuspendRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameControllerSuspendRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate_word(Nba97GameControllerSuspendRun* run,
    uint32_t address, uint32_t pc, uint8_t** data, uint8_t** known) {
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

static int read_word(Nba97GameControllerSuspendRun* run, uint32_t address,
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

static int write_word(Nba97GameControllerSuspendRun* run, uint32_t address,
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

static int invoke_shutdown(Nba97GameControllerSuspendRun* run,
    Nba97GameControllerSuspendValue* value) {
    Nba97GameControllerSuspendEvent event;
    int result;
    stop(run, 0x8008f1b0u, 0, SHUTDOWN_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x8008f1b0u;
    event.entry = SHUTDOWN_ENTRY;
    event.stack_pointer = run->sp;
    event.return_address = 0x8008f1b8u;
    event.kind = NBA97_GAME_CONTROLLER_SUSPEND_SHUTDOWN;
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

static int validate(Nba97GameControllerSuspendContext* context,
    Nba97GameControllerSuspendProgress* out,
    Nba97GameControllerSuspendRun* run) {
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
    run->sp = context->stack_pointer;
    out->frame_stack_pointer = context->stack_pointer - 0x18u;
    out->stack_pointer = context->stack_pointer;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_controller_suspend(Nba97GameControllerSuspendContext* context,
    Nba97GameControllerSuspendProgress* out) {
    Nba97GameControllerSuspendRun storage;
    Nba97GameControllerSuspendRun* run = &storage;
    Nba97GameControllerSuspendValue child_value;
    uint32_t suspended;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8008F19C..0x8008F1D3: the flag load at 0x8008F1A0
     * happens before addiu sp,-0x18 and the unconditional branch-delay spill
     * at 0x8008F1AC. Keep that order when the two addresses alias. */
    TRY(read_word(run, SUSPEND_FLAG_ADDRESS, 0x8008f1a0u, &suspended));
    out->initial_suspend_flag = suspended;
    run->sp -= 0x18u;
    out->stack_pointer = run->sp;
    TRY(write_word(run, run->sp + 0x10u, 0x8008f1acu,
        context->return_address));

    if (!suspended) {
        TRY(invoke_shutdown(run, &child_value));
        out->shutdown_called = 1;
        /* The source immediately overwrites even unknown child v0 with one. */
        out->return_v0 = 1;
        out->return_v0_known = 1;
        TRY(write_word(run, SUSPEND_FLAG_ADDRESS, 0x8008f1c0u, 1));
        out->input_suspended = 1;
    } else {
        /* Preserve the exact loaded nonzero word; the source does not make it
         * boolean on its already-suspended path. */
        out->return_v0 = suspended;
        out->return_v0_known = 1;
        out->input_suspended = 1;
    }

    /* A child may have changed the saved word; reload ra from live memory. */
    TRY(read_word(run, run->sp + 0x10u, 0x8008f1c4u,
        &out->restored_return_address));
    run->sp += 0x18u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
