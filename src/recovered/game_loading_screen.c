#include "game_loading_screen.h"

#include <string.h>

#define LOAD_RESOURCE_ENTRY UINT32_C(0x80029bfc)
#define FIND_IMAGE_ENTRY UINT32_C(0x800a5478)
#define DRAW_SYNC_ENTRY UINT32_C(0x800994f4)
#define UPLOAD_IMAGE_ENTRY UINT32_C(0x800946b8)
#define RELEASE_RESOURCE_ENTRY UINT32_C(0x80090698)

#define RESOURCE_NAME_ADDRESS UINT32_C(0x800247f8)
#define IMAGE_NAME_ADDRESS UINT32_C(0x80024808)

typedef struct Nba97GameLoadingScreenRun {
    Nba97GameLoadingScreenContext* context;
    Nba97GameLoadingScreenProgress* out;
    uint32_t sp;
    uint32_t s0;
    uint32_t s1;
    uint8_t s0_known;
    uint8_t s1_known;
} Nba97GameLoadingScreenRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameLoadingScreenRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameLoadingScreenRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate_word(Nba97GameLoadingScreenRun* run, uint32_t address,
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

static int write_word(Nba97GameLoadingScreenRun* run, uint32_t address,
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

static int read_word(Nba97GameLoadingScreenRun* run, uint32_t address,
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

static int validate(Nba97GameLoadingScreenContext* context,
    Nba97GameLoadingScreenProgress* out, Nba97GameLoadingScreenRun* run) {
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
    run->sp = context->stack_pointer - 0x28u;
    run->s0 = context->saved_register[0];
    run->s1 = context->saved_register[1];
    run->s0_known = 1;
    run->s1_known = 1;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameLoadingScreenRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count, uint32_t a0,
    uint32_t a1, uint32_t a2, uint32_t a3, uint32_t stack_a4,
    Nba97GameLoadingScreenValue* value) {
    Nba97GameLoadingScreenEvent event;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.argument[0] = a0;
    event.argument[1] = a1;
    event.argument[2] = a2;
    event.argument[3] = a3;
    event.argument[4] = stack_a4;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.saved_register[0] = run->s0;
    event.saved_register[1] = run->s1;
    event.return_address = pc + 8u;
    event.kind = kind;
    event.argument_count = argument_count;
    event.saved_register_known[0] = run->s0_known;
    event.saved_register_known[1] = run->s1_known;
    value->word = 0;
    value->known = 0;
    result = run->context->io(run->context->user, &run->context->memory,
        &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    switch (kind) {
    case NBA97_GAME_LOADING_SCREEN_LOAD_RESOURCE: ++run->out->load_calls; break;
    case NBA97_GAME_LOADING_SCREEN_FIND_IMAGE: ++run->out->lookup_calls; break;
    case NBA97_GAME_LOADING_SCREEN_DRAW_SYNC: ++run->out->draw_sync_calls; break;
    case NBA97_GAME_LOADING_SCREEN_UPLOAD_IMAGE: ++run->out->upload_calls; break;
    case NBA97_GAME_LOADING_SCREEN_RELEASE_RESOURCE: ++run->out->release_calls; break;
    default: return NBA97_TEXT_ARGUMENT;
    }
    return NBA97_TEXT_COMPLETE;
}

static int draw_sync(Nba97GameLoadingScreenRun* run, uint32_t pc,
    Nba97GameLoadingScreenValue* value) {
    return invoke(run, pc, DRAW_SYNC_ENTRY,
        NBA97_GAME_LOADING_SCREEN_DRAW_SYNC, 1, 0, 0, 0, 0, 0, value);
}

static int upload(Nba97GameLoadingScreenRun* run, uint32_t pc,
    uint32_t x, uint32_t y, Nba97GameLoadingScreenValue* value) {
    return invoke(run, pc, UPLOAD_IMAGE_ENTRY,
        NBA97_GAME_LOADING_SCREEN_UPLOAD_IMAGE, 5, run->s0, x, y, 0, 0,
        value);
}

int nba97_game_loading_screen(Nba97GameLoadingScreenContext* context,
    Nba97GameLoadingScreenProgress* out) {
    Nba97GameLoadingScreenRun storage;
    Nba97GameLoadingScreenRun* run = &storage;
    Nba97GameLoadingScreenValue value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80029E58 prologue. The s0 spill at 0x80029E74 is the
     * resource-loader JAL delay slot, so the callee observes all three saves. */
    TRY(write_word(run, run->sp + 0x20u, 0x80029e68u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x1cu, 0x80029e6cu, run->s1));
    TRY(write_word(run, run->sp + 0x18u, 0x80029e74u, run->s0));
    TRY(invoke(run, 0x80029e70u, LOAD_RESOURCE_ENTRY,
        NBA97_GAME_LOADING_SCREEN_LOAD_RESOURCE, 2, RESOURCE_NAME_ADDRESS,
        0, 0, 0, 0, &value));
    if (!value.known) {
        stop(run, 0x80029e7cu, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    run->s1 = value.word;
    run->s1_known = value.known;
    out->loaded_resource = run->s1;
    if (!run->s1) {
        /* Original BEQ at 0x80029E7C: failed load is a normal silent return.
         * Do not turn it into a host-I/O error or attempt a compensating free. */
        out->skipped_for_null_resource = 1;
        goto epilogue;
    }
    out->resource_loaded = 1;

    TRY(invoke(run, 0x80029e8cu, FIND_IMAGE_ENTRY,
        NBA97_GAME_LOADING_SCREEN_FIND_IMAGE, 2, run->s1,
        IMAGE_NAME_ADDRESS, 0, 0, 0, &value));
    run->s0 = value.word;
    run->s0_known = value.known;
    out->resolved_image = run->s0;
    out->resolved_image_known = run->s0_known;
    out->image_lookup_completed = 1;

    /* GAMEONLY 0x80029E98..0x80029EFC. All coordinates are full o32 words;
     * every fifth upload argument is the zero written to sp+0x10 in the JAL
     * delay slot. There is intentionally no guard for a zero image pointer. */
    TRY(draw_sync(run, 0x80029e98u, &value));
    if (!run->s0_known) {
        stop(run, 0x80029ea0u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    TRY(upload(run, 0x80029eb0u, 0, 0, &value));
    TRY(draw_sync(run, 0x80029eb8u, &value));
    TRY(upload(run, 0x80029ed0u, 0, 0x100u, &value));
    TRY(draw_sync(run, 0x80029ed8u, &value));
    TRY(upload(run, 0x80029ef0u, 0x200u, 0, &value));
    TRY(draw_sync(run, 0x80029ef8u, &value));
    TRY(invoke(run, 0x80029f00u, RELEASE_RESOURCE_ENTRY,
        NBA97_GAME_LOADING_SCREEN_RELEASE_RESOURCE, 1, run->s1,
        0, 0, 0, 0, &value));

epilogue:
    /* 0x80029F08..0x80029F10 reload mutable stack storage in this order.
     * The last child's raw v0 is not normalized by the nominally-void owner. */
    TRY(read_word(run, run->sp + 0x20u, 0x80029f08u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x1cu, 0x80029f0cu, &run->s1));
    TRY(read_word(run, run->sp + 0x18u, 0x80029f10u, &run->s0));
    out->restored_saved_register[0] = run->s0;
    out->restored_saved_register[1] = run->s1;
    out->stack_pointer = run->sp + 0x28u;
    out->return_v0 = value.word;
    out->return_v0_known = value.known;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
