#include "game_clock_shutdown.h"

#include <string.h>

#define INTERRUPT_CALLBACK_ENTRY UINT32_C(0x8009860c)

typedef struct Nba97GameClockShutdownRun {
    Nba97GameClockShutdownContext* context;
    Nba97GameClockShutdownProgress* out;
    uint32_t sp;
    uint32_t fp;
} Nba97GameClockShutdownRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameClockShutdownRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameClockShutdownRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate_word(Nba97GameClockShutdownRun* run, uint32_t address,
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

static int write_word(Nba97GameClockShutdownRun* run, uint32_t address,
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

static int read_word(Nba97GameClockShutdownRun* run, uint32_t address,
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

static int invoke_interrupt_callback(Nba97GameClockShutdownRun* run,
    Nba97GameClockShutdownValue* value) {
    Nba97GameClockShutdownEvent event;
    int result;
    stop(run, 0x80091694u, 0, INTERRUPT_CALLBACK_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x80091694u;
    event.entry = INTERRUPT_CALLBACK_ENTRY;
    event.argument[0] = 6;
    event.argument[1] = 0;
    event.stack_pointer = run->sp;
    event.frame_pointer = run->fp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = 0x8009169cu;
    event.kind = NBA97_GAME_CLOCK_SHUTDOWN_INTERRUPT_CALLBACK;
    event.argument_count = 2;
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

static int validate(Nba97GameClockShutdownContext* context,
    Nba97GameClockShutdownProgress* out,
    Nba97GameClockShutdownRun* run) {
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
    run->fp = context->frame_pointer;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->incoming_frame_pointer = context->frame_pointer;
    out->interrupt_callback_entry = INTERRUPT_CALLBACK_ENTRY;
    out->interrupt_number = 6;
    out->replacement_callback = 0;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_clock_shutdown(Nba97GameClockShutdownContext* context,
    Nba97GameClockShutdownProgress* out) {
    Nba97GameClockShutdownRun storage;
    Nba97GameClockShutdownRun* run = &storage;
    Nba97GameClockShutdownValue value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8009167C..0x800916B3: retain both saved words and their
     * source-order live reloads. InterruptCallback's incidental v0 is not
     * overwritten by this nominally void wrapper. */
    TRY(write_word(run, run->sp + 0x14u, 0x80091680u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x10u, 0x80091684u,
        context->frame_pointer));
    run->fp = run->sp;
    TRY(invoke_interrupt_callback(run, &value));
    out->return_v0 = value.word;
    out->return_v0_known = value.known;
    run->sp = run->fp;
    TRY(read_word(run, run->sp + 0x14u, 0x800916a0u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x10u, 0x800916a4u,
        &run->fp));
    run->sp += 0x18u;
    out->stack_pointer = run->sp;
    out->restored_frame_pointer = run->fp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
