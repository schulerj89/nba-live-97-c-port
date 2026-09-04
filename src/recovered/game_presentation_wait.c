#include "game_presentation_wait.h"

#include <string.h>

#define PRESENTATION_WAIT_ENTRY UINT32_C(0x800a9cc0)

typedef struct Nba97GamePresentationWaitRun {
    Nba97GamePresentationWaitContext* context;
    Nba97GamePresentationWaitProgress* out;
    uint32_t sp;
} Nba97GamePresentationWaitRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GamePresentationWaitRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GamePresentationWaitRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GamePresentationWaitRun* run, uint32_t address,
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

static int write_word(Nba97GamePresentationWaitRun* run, uint32_t address,
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

static int read_word(Nba97GamePresentationWaitRun* run, uint32_t address,
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

static int invoke_wait(Nba97GamePresentationWaitRun* run,
    Nba97GamePresentationWaitValue* value) {
    Nba97GamePresentationWaitEvent event;
    int result;
    stop(run, 0x80029be4u, 0, PRESENTATION_WAIT_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x80029be4u;
    event.entry = PRESENTATION_WAIT_ENTRY;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = 0x80029becu;
    event.kind = NBA97_GAME_PRESENTATION_WAIT_SERVICE;
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

static int validate(Nba97GamePresentationWaitContext* context,
    Nba97GamePresentationWaitProgress* out,
    Nba97GamePresentationWaitRun* run) {
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
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->service_entry = PRESENTATION_WAIT_ENTRY;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_presentation_wait(Nba97GamePresentationWaitContext* context,
    Nba97GamePresentationWaitProgress* out) {
    Nba97GamePresentationWaitRun storage;
    Nba97GamePresentationWaitRun* run = &storage;
    Nba97GamePresentationWaitValue value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80029BDC..0x80029BFB. Keep the real frame and live ra reload:
     * the child may alias or deliberately rewrite the saved word. */
    TRY(write_word(run, run->sp + 0x10u, 0x80029be0u,
        context->return_address));
    TRY(invoke_wait(run, &value));
    out->return_v0 = value.word;
    out->return_v0_known = value.known;
    TRY(read_word(run, run->sp + 0x10u, 0x80029becu,
        &out->restored_return_address));
    run->sp += 0x18u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
