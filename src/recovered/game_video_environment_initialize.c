#include "game_video_environment_initialize.h"

#include <string.h>

#define DRAW_ENV_0 UINT32_C(0x80021eec)
#define DRAW_ENV_1 UINT32_C(0x80021f48)
#define DISP_ENV_0 UINT32_C(0x8002205c)
#define DISP_ENV_1 UINT32_C(0x80022070)
#define BUFFER_SELECTOR UINT32_C(0x8001ede8)
#define SET_DEF_DRAW_ENV_ENTRY UINT32_C(0x8009ca00)
#define SET_DEF_DISP_ENV_ENTRY UINT32_C(0x8009cad0)
#define PUT_DRAW_ENV_ENTRY UINT32_C(0x80099acc)
#define PUT_DISP_ENV_ENTRY UINT32_C(0x80099ca4)
#define DRAW_SYNC_ENTRY UINT32_C(0x800994f4)

typedef struct Nba97GameVideoEnvironmentInitializeRun {
    Nba97GameVideoEnvironmentInitializeContext* context;
    Nba97GameVideoEnvironmentInitializeProgress* out;
    uint32_t sp;
    uint32_t s[6];
} Nba97GameVideoEnvironmentInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameVideoEnvironmentInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameVideoEnvironmentInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameVideoEnvironmentInitializeRun* run,
    uint32_t address, size_t width, size_t alignment, uint32_t pc,
    uint8_t** data, uint8_t** known) {
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

static int write_value(Nba97GameVideoEnvironmentInitializeRun* run,
    uint32_t address, size_t width, size_t alignment, uint32_t pc,
    uint32_t value) {
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

static int write_word(Nba97GameVideoEnvironmentInitializeRun* run,
    uint32_t address, uint32_t pc, uint32_t value) {
    return write_value(run, address, 4, 4, pc, value);
}

static int write_byte(Nba97GameVideoEnvironmentInitializeRun* run,
    uint32_t address, uint32_t pc, uint8_t value) {
    int result = write_value(run, address, 1, 1, pc, value);
    if (result == NBA97_TEXT_COMPLETE)
        ++run->out->direct_control_bytes_written;
    return result;
}

static int read_word(Nba97GameVideoEnvironmentInitializeRun* run,
    uint32_t address, uint32_t pc, uint32_t* value) {
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

static int invoke(Nba97GameVideoEnvironmentInitializeRun* run,
    uint32_t pc, uint32_t entry, uint8_t kind, uint8_t argument_count,
    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
    Nba97GameVideoEnvironmentInitializeValue* value) {
    Nba97GameVideoEnvironmentInitializeEvent event;
    int result;
    unsigned i;
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
    event.argument[4] = a4;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    for (i = 0; i < 6; ++i)
        event.saved_register[i] = run->s[i];
    event.return_address = pc + 8u;
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

static int validate(Nba97GameVideoEnvironmentInitializeContext* context,
    Nba97GameVideoEnvironmentInitializeProgress* out,
    Nba97GameVideoEnvironmentInitializeRun* run) {
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
    run->sp = context->stack_pointer - 0x38u;
    for (i = 0; i < 6; ++i)
        run->s[i] = context->saved_register[i];
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->requested_background_mode = context->background_mode;
    out->background_byte = (uint8_t)context->background_mode;
    out->display_environment[0] = DISP_ENV_0;
    out->display_environment[1] = DISP_ENV_1;
    out->draw_environment[0] = DRAW_ENV_0;
    out->draw_environment[1] = DRAW_ENV_1;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_video_environment_initialize(
    Nba97GameVideoEnvironmentInitializeContext* context,
    Nba97GameVideoEnvironmentInitializeProgress* out) {
    Nba97GameVideoEnvironmentInitializeRun storage;
    Nba97GameVideoEnvironmentInitializeRun* run = &storage;
    Nba97GameVideoEnvironmentInitializeValue value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80029F20 prologue. Preserve the noncanonical save order;
     * children can alias these live mapped words before the epilogue reloads. */
    TRY(write_word(run, run->sp + 0x1cu, 0x80029f24u, run->s[1]));
    run->s[1] = context->background_mode;
    TRY(write_word(run, run->sp + 0x20u, 0x80029f2cu, run->s[2]));
    run->s[2] = DISP_ENV_0;
    TRY(write_word(run, run->sp + 0x18u, 0x80029f48u, run->s[0]));
    run->s[0] = 0xf0u;
    TRY(write_word(run, run->sp + 0x30u, 0x80029f50u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x2cu, 0x80029f54u, run->s[5]));
    TRY(write_word(run, run->sp + 0x28u, 0x80029f58u, run->s[4]));
    TRY(write_word(run, run->sp + 0x24u, 0x80029f5cu, run->s[3]));

    /* All four fifth arguments are real o32 stack writes in JAL delay slots.
     * The callbacks run only after those source instructions have executed. */
    TRY(write_word(run, run->sp + 0x10u, 0x80029f64u, run->s[0]));
    TRY(invoke(run, 0x80029f60u, SET_DEF_DISP_ENV_ENTRY,
        NBA97_GAME_VIDEO_SET_DEF_DISP_ENV, 5,
        run->s[2], 0, 0x100u, 0x200u, run->s[0], &value));
    run->s[5] = DISP_ENV_1;
    TRY(write_word(run, run->sp + 0x10u, 0x80029f80u, run->s[0]));
    TRY(invoke(run, 0x80029f7cu, SET_DEF_DISP_ENV_ENTRY,
        NBA97_GAME_VIDEO_SET_DEF_DISP_ENV, 5,
        run->s[5], 0, 0, 0x200u, run->s[0], &value));
    run->s[3] = DRAW_ENV_0;
    TRY(write_word(run, run->sp + 0x10u, 0x80029fa0u, run->s[0]));
    TRY(invoke(run, 0x80029f9cu, SET_DEF_DRAW_ENV_ENTRY,
        NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV, 5,
        run->s[3], 0, 0, 0x200u, run->s[0], &value));
    run->s[4] = DRAW_ENV_1;
    TRY(write_word(run, run->sp + 0x10u, 0x80029fbcu, run->s[0]));
    TRY(invoke(run, 0x80029fb8u, SET_DEF_DRAW_ENV_ENTRY,
        NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV, 5,
        run->s[4], 0, 0x100u, 0x200u, run->s[0], &value));

    /* 0x80029FC0..0x8002A03C. The first eight stores deliberately touch
     * DRAWENV[3], [2], [1], [0] in reverse address order. Records 2 and 3
     * were not initialized above; only these named bytes become known. */
    TRY(write_byte(run, 0x80022016u, 0x80029fc4u, 0));
    TRY(write_byte(run, 0x80021fbau, 0x80029fccu, 0));
    TRY(write_byte(run, 0x80021f5eu, 0x80029fd4u, 0));
    TRY(write_byte(run, 0x80021f02u, 0x80029fdcu, 0));
    TRY(write_byte(run, 0x80022018u, 0x80029fe4u,
        (uint8_t)run->s[1]));
    TRY(write_byte(run, 0x80021fbcu, 0x80029fecu,
        (uint8_t)run->s[1]));
    TRY(write_byte(run, 0x80021f60u, 0x80029ff4u,
        (uint8_t)run->s[1]));
    TRY(write_byte(run, 0x80021f04u, 0x80029ffcu,
        (uint8_t)run->s[1]));
    TRY(write_byte(run, 0x80022081u, 0x8002a004u, 0));
    TRY(write_byte(run, 0x8002206du, 0x8002a00cu, 0));
    TRY(write_byte(run, 0x80021f05u, 0x8002a014u, 0));
    TRY(write_byte(run, 0x80021f06u, 0x8002a01cu, 0));
    TRY(write_byte(run, 0x80021f07u, 0x8002a024u, 0));
    TRY(write_byte(run, 0x80021f61u, 0x8002a02cu, 0));
    TRY(write_byte(run, 0x80021f62u, 0x8002a034u, 0));
    TRY(write_byte(run, 0x80021f63u, 0x8002a03cu, 0));

    /* Both pairs are installed in source order. Pair 1 is therefore the last
     * active hardware pair even though the software selector is cleared. */
    TRY(invoke(run, 0x8002a040u, PUT_DISP_ENV_ENTRY,
        NBA97_GAME_VIDEO_PUT_DISP_ENV, 1,
        run->s[2], 0, 0, 0, 0, &value));
    TRY(invoke(run, 0x8002a048u, PUT_DRAW_ENV_ENTRY,
        NBA97_GAME_VIDEO_PUT_DRAW_ENV, 1,
        run->s[3], 0, 0, 0, 0, &value));
    TRY(invoke(run, 0x8002a050u, PUT_DISP_ENV_ENTRY,
        NBA97_GAME_VIDEO_PUT_DISP_ENV, 1,
        run->s[5], 0, 0, 0, 0, &value));
    TRY(invoke(run, 0x8002a058u, PUT_DRAW_ENV_ENTRY,
        NBA97_GAME_VIDEO_PUT_DRAW_ENV, 1,
        run->s[4], 0, 0, 0, 0, &value));
    TRY(invoke(run, 0x8002a060u, DRAW_SYNC_ENTRY,
        NBA97_GAME_VIDEO_DRAW_SYNC, 1, 0, 0, 0, 0, 0, &value));
    out->return_v0 = value.word;
    out->return_v0_known = value.known;
    TRY(write_word(run, BUFFER_SELECTOR, 0x8002a06cu, 0));

    /* 0x8002A070..0x8002A094 reloads every saved word from live memory. */
    TRY(read_word(run, run->sp + 0x30u, 0x8002a070u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x2cu, 0x8002a074u,
        &out->restored_saved_register[5]));
    TRY(read_word(run, run->sp + 0x28u, 0x8002a078u,
        &out->restored_saved_register[4]));
    TRY(read_word(run, run->sp + 0x24u, 0x8002a07cu,
        &out->restored_saved_register[3]));
    TRY(read_word(run, run->sp + 0x20u, 0x8002a080u,
        &out->restored_saved_register[2]));
    TRY(read_word(run, run->sp + 0x1cu, 0x8002a084u,
        &out->restored_saved_register[1]));
    TRY(read_word(run, run->sp + 0x18u, 0x8002a088u,
        &out->restored_saved_register[0]));
    run->sp += 0x38u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
