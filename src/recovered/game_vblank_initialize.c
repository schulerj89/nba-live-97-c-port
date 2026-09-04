#include "game_vblank_initialize.h"

#include <string.h>

#define CALLBACK_TABLE UINT32_C(0x800d6e0c)
#define GLOBAL_POINTER_SAVE_ENTRY UINT32_C(0x800a4830)
#define DRAW_SYNC_ENTRY UINT32_C(0x800994f4)
#define ENTER_CRITICAL_SECTION_ENTRY UINT32_C(0x80098394)
#define INTERRUPT_CALLBACK_ENTRY UINT32_C(0x8009860c)
#define SET_RCNT_ENTRY UINT32_C(0x800983b4)
#define START_RCNT_ENTRY UINT32_C(0x80098488)
#define EXIT_CRITICAL_SECTION_ENTRY UINT32_C(0x80098594)
#define FRAME_COUNTER_RESET_ENTRY UINT32_C(0x800a3e48)
#define VBLANK_HANDLER UINT32_C(0x800a450c)
#define VBLANK_COUNTER_SPEC UINT32_C(0xf2000003)
#define VBLANK_COUNTER_TARGET UINT32_C(1)
#define VBLANK_COUNTER_MODE UINT32_C(0x1000)

typedef struct Nba97GameVblankInitializeRun {
    Nba97GameVblankInitializeContext* context;
    Nba97GameVblankInitializeProgress* out;
    uint32_t sp;
    uint32_t fp;
} Nba97GameVblankInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameVblankInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameVblankInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameVblankInitializeRun* run, uint32_t address,
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

static int read_word(Nba97GameVblankInitializeRun* run, uint32_t address,
    uint32_t pc, uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
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

static int write_word(Nba97GameVblankInitializeRun* run, uint32_t address,
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

static int direct_call(Nba97GameVblankInitializeRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count, uint32_t a0, uint32_t a1,
    uint32_t a2, Nba97GameVblankInitializeValue* value) {
    Nba97GameVblankInitializeEvent event;
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
    event.stack_pointer = run->sp;
    event.frame_pointer = run->fp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = pc + 8u;
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

static int validate(Nba97GameVblankInitializeContext* context,
    Nba97GameVblankInitializeProgress* out,
    Nba97GameVblankInitializeRun* run) {
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
    run->fp = context->frame_pointer;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->interrupt_handler = VBLANK_HANDLER;
    out->root_counter_spec = VBLANK_COUNTER_SPEC;
    out->root_counter_target = VBLANK_COUNTER_TARGET;
    out->root_counter_mode = VBLANK_COUNTER_MODE;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_vblank_initialize(Nba97GameVblankInitializeContext* context,
    Nba97GameVblankInitializeProgress* out) {
    Nba97GameVblankInitializeRun storage;
    Nba97GameVblankInitializeRun* run = &storage;
    Nba97GameVblankInitializeValue value;
    uint32_t index;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800A43E8 prologue. The original frame is observable because
     * both saved words are reloaded after every child returns. */
    TRY(write_word(run, run->sp + 0x1cu, 0x800a43ecu,
        context->return_address));
    TRY(write_word(run, run->sp + 0x18u, 0x800a43f0u, run->fp));
    run->fp = run->sp;
    TRY(direct_call(run, 0x800a43f8u, GLOBAL_POINTER_SAVE_ENTRY, 0,
        0, 0, 0, &value));

    /* 0x800A4400..0x800A445B keeps the induction variable in the stack
     * frame. Preserve all three loads and both stores for each of eight words. */
    TRY(write_word(run, run->fp + 0x10u, 0x800a4400u, 0));
    for (;;) {
        TRY(read_word(run, run->fp + 0x10u, 0x800a4404u, &index));
        if ((int32_t)index >= 8)
            break;
        TRY(read_word(run, run->fp + 0x10u, 0x800a4420u, &index));
        TRY(write_word(run, CALLBACK_TABLE + index * 4u,
            0x800a443cu, 0));
        ++out->callback_slots_cleared;
        TRY(read_word(run, run->fp + 0x10u, 0x800a4440u, &index));
        TRY(write_word(run, run->fp + 0x10u, 0x800a4450u,
            index + 1u));
    }

    /* GAMEONLY 0x800A445C..0x800A44B8. These are explicit platform/service
     * boundaries; the native renderer and scheduler are not touched here. */
    TRY(direct_call(run, 0x800a4460u, DRAW_SYNC_ENTRY, 1,
        0, 0, 0, &value));
    TRY(direct_call(run, 0x800a4468u, ENTER_CRITICAL_SECTION_ENTRY, 0,
        0, 0, 0, &value));
    TRY(direct_call(run, 0x800a447cu, INTERRUPT_CALLBACK_ENTRY, 2,
        0, VBLANK_HANDLER, 0, &value));

    /* Retail passes low-half counter index 3. PsyQ SetRCnt returns false for
     * it; StartRCnt still unmasks VBlank and also returns false. Both v0 values
     * are deliberately recorded and then ignored, matching 0x800A449C/A8. */
    TRY(direct_call(run, 0x800a4494u, SET_RCNT_ENTRY, 3,
        VBLANK_COUNTER_SPEC, VBLANK_COUNTER_TARGET,
        VBLANK_COUNTER_MODE, &value));
    out->set_rcnt_return = value.word;
    out->set_rcnt_return_known = value.known;
    TRY(direct_call(run, 0x800a44a4u, START_RCNT_ENTRY, 1,
        VBLANK_COUNTER_SPEC, 0, 0, &value));
    out->start_rcnt_return = value.word;
    out->start_rcnt_return_known = value.known;
    TRY(direct_call(run, 0x800a44acu, EXIT_CRITICAL_SECTION_ENTRY, 0,
        0, 0, 0, &value));
    TRY(direct_call(run, 0x800a44b4u, FRAME_COUNTER_RESET_ENTRY, 0,
        0, 0, 0, &value));
    out->return_v0 = value.word;
    out->return_v0_known = value.known;

    /* GAMEONLY 0x800A44BC..0x800A44D3 reloads the live saved stack words. */
    run->sp = run->fp;
    TRY(read_word(run, run->sp + 0x1cu, 0x800a44c0u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x18u, 0x800a44c4u,
        &out->restored_frame_pointer));
    run->sp += 0x20u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
