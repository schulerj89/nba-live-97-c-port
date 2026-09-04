#include "game_controller_resume.h"

#include <string.h>

#define SUSPEND_FLAG_ADDRESS UINT32_C(0x800c4a70)
#define CLOCK_SNAPSHOT_ADDRESS UINT32_C(0x800c4a74)
#define PAD_MODE_ADDRESS UINT32_C(0x800d7a48)

typedef struct Nba97GameControllerResumeRun {
    Nba97GameControllerResumeContext* context;
    Nba97GameControllerResumeProgress* out;
    uint32_t sp;
} Nba97GameControllerResumeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameControllerResumeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameControllerResumeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameControllerResumeRun* run, uint32_t address,
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

static int read_word(Nba97GameControllerResumeRun* run, uint32_t address,
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

static int write_value(Nba97GameControllerResumeRun* run,
    uint32_t address, uint32_t pc, uint32_t value, uint8_t value_known) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    if (value_known > 1)
        return NBA97_TEXT_ARGUMENT;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && !value_known)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = value_known;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameControllerResumeRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_value(run, address, pc, value, 1);
}

static int callback(Nba97GameControllerResumeRun* run, uint8_t kind,
    uint32_t pc, uint32_t entry, Nba97GameControllerResumeValue* value) {
    Nba97GameControllerResumeEvent event;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.stack_pointer = run->sp;
    event.return_address = pc + 8u;
    event.kind = kind;
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

static int validate(Nba97GameControllerResumeContext* context,
    Nba97GameControllerResumeProgress* out,
    Nba97GameControllerResumeRun* run) {
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
    out->requested_pad_mode = context->pad_mode;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_controller_resume(Nba97GameControllerResumeContext* context,
    Nba97GameControllerResumeProgress* out) {
    Nba97GameControllerResumeRun storage;
    Nba97GameControllerResumeRun* run = &storage;
    Nba97GameControllerResumeValue value;
    uint32_t restored;
    uint32_t suspended;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8008F1D8 reads the suspend flag before allocating/spilling
     * the frame. Keep that source order: a deliberately aliased stack spill
     * cannot retroactively change which branch the original takes. */
    TRY(read_word(run, SUSPEND_FLAG_ADDRESS, 0x8008f1d8u, &suspended));
    out->initial_suspend_flag = suspended;
    TRY(write_word(run, run->sp + 0x10u, 0x8008f1e0u,
        context->return_address));
    TRY(write_word(run, PAD_MODE_ADDRESS, 0x8008f1e8u,
        context->pad_mode));

    if (suspended) {
        TRY(callback(run, NBA97_GAME_CONTROLLER_RESUME_INITIALIZE,
            0x8008f1f4u, 0x80091184u, &value));
        out->input_reinitialized = 1;
        TRY(write_word(run, SUSPEND_FLAG_ADDRESS, 0x8008f200u, 0));
        TRY(callback(run, NBA97_GAME_CONTROLLER_RESUME_CLOCK,
            0x8008f204u, 0x800a5810u, &value));
        out->clock_snapshot = value.word;
        out->clock_snapshot_known = value.known;
        out->return_v0 = value.word;
        out->return_v0_known = value.known;
        TRY(write_value(run, CLOCK_SNAPSHOT_ADDRESS, 0x8008f210u,
            value.word, value.known));
    } else {
        /* v0 still contains the zero loaded from 0x800C4A70. */
        out->return_v0 = 0;
        out->return_v0_known = 1;
    }

    /* Either child may alter the saved word, so restore ra from live memory. */
    TRY(read_word(run, run->sp + 0x10u, 0x8008f214u, &restored));
    out->restored_return_address = restored;
    out->stack_pointer = run->sp + 0x18u;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
