#include "game_match_session.h"

#include <string.h>

#define CLEAR_RECTANGLE_ENTRY UINT32_C(0x800aa0bc)
#define FRAME_RATE_RESET_ENTRY UINT32_C(0x800a7738)
#define SET_DEF_DRAW_ENV_ENTRY UINT32_C(0x8009ca00)
#define SET_DEF_DISP_ENV_ENTRY UINT32_C(0x8009cad0)
#define LOCATION_LOOKUP_ENTRY UINT32_C(0x80081b50)
#define INITIALIZE_ENTRY UINT32_C(0x8002db90)
#define LOAD_SCENE_ENTRY UINT32_C(0x8002db68)
#define RUN_LOOP_ENTRY UINT32_C(0x8002dc38)
#define TEARDOWN_ENTRY UINT32_C(0x8002dc58)
#define PRESENTATION_WAIT_ENTRY UINT32_C(0x80029bdc)
#define DRAW_SYNC_ENTRY UINT32_C(0x800994f4)

#define CUSTOM_LOCATION_ADDRESS UINT32_C(0x8001ec94)
#define TEAM_INDEX_ADDRESS UINT32_C(0x80021d74)
#define TEAM_RECORD_BASE UINT32_C(0x80023af8)

typedef struct Nba97GameMatchSessionRawWord {
    uint32_t word;
    uint8_t known[4];
} Nba97GameMatchSessionRawWord;

typedef struct Nba97GameMatchSessionRun {
    Nba97GameMatchSessionContext* context;
    Nba97GameMatchSessionProgress* out;
    uint32_t sp;
    uint32_t s[3];
} Nba97GameMatchSessionRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameMatchSessionRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameMatchSessionRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameMatchSessionRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
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

static int write_known(Nba97GameMatchSessionRun* run, uint32_t address,
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

static int write_word(Nba97GameMatchSessionRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_known(run, address, 4, 4, pc, value);
}

static int write_half(Nba97GameMatchSessionRun* run, uint32_t address,
    uint32_t pc, uint16_t value) {
    return write_known(run, address, 2, 2, pc, value);
}

static int write_byte(Nba97GameMatchSessionRun* run, uint32_t address,
    uint32_t pc, uint8_t value) {
    int result = write_known(run, address, 1, 1, pc, value);
    if (result == NBA97_TEXT_COMPLETE)
        ++run->out->direct_control_bytes_written;
    return result;
}

static int read_raw_word(Nba97GameMatchSessionRun* run, uint32_t address,
    uint32_t pc, Nba97GameMatchSessionRawWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    value->word = 0;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        value->word |= (uint32_t)data[i] << (i * 8u);
        value->known[i] = known ? known[i] : 1;
    }
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static uint8_t raw_known_mask(const Nba97GameMatchSessionRawWord* value) {
    uint8_t result = 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (value->known[i])
            result = (uint8_t)(result | (uint8_t)(1u << i));
    return result;
}

static int require_known(Nba97GameMatchSessionRun* run,
    const Nba97GameMatchSessionRawWord* value, uint32_t pc,
    uint32_t address) {
    if (raw_known_mask(value) != 0x0fu) {
        stop(run, pc, address, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int read_required_word(Nba97GameMatchSessionRun* run,
    uint32_t address, uint32_t load_pc, uint32_t use_pc, uint32_t* value) {
    Nba97GameMatchSessionRawWord raw;
    TRY(read_raw_word(run, address, load_pc, &raw));
    TRY(require_known(run, &raw, use_pc, address));
    *value = raw.word;
    return NBA97_TEXT_COMPLETE;
}

static int write_raw_word(Nba97GameMatchSessionRun* run, uint32_t address,
    uint32_t pc, const Nba97GameMatchSessionRawWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (!known && raw_known_mask(value) != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (i * 8u));
        if (known)
            known[i] = value->known[i];
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameMatchSessionRawWord callback_word(
    const Nba97GameMatchSessionValue* value) {
    Nba97GameMatchSessionRawWord raw;
    unsigned i;
    raw.word = value->word;
    for (i = 0; i < 4; ++i)
        raw.known[i] = value->known;
    return raw;
}

static uint32_t team_record_address(uint32_t index) {
    uint32_t offset = index << 1;
    offset += index;
    offset <<= 2;
    offset += index;
    offset <<= 3;
    return TEAM_RECORD_BASE + offset;
}

static int invoke(Nba97GameMatchSessionRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
    Nba97GameMatchSessionValue* value) {
    Nba97GameMatchSessionEvent event;
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
    for (i = 0; i < 3; ++i)
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

static int counted_invoke(Nba97GameMatchSessionRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
    size_t* count, Nba97GameMatchSessionValue* value) {
    TRY(invoke(run, pc, entry, kind, argument_count,
        a0, a1, a2, a3, a4, value));
    ++*count;
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameMatchSessionContext* context,
    Nba97GameMatchSessionProgress* out, Nba97GameMatchSessionRun* run) {
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
    for (i = 0; i < 3; ++i)
        run->s[i] = context->saved_register[i];
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_session(Nba97GameMatchSessionContext* context,
    Nba97GameMatchSessionProgress* out) {
    Nba97GameMatchSessionRun storage;
    Nba97GameMatchSessionRun* run = &storage;
    Nba97GameMatchSessionValue value;
    Nba97GameMatchSessionRawWord custom;
    Nba97GameMatchSessionRawWord saved[2];
    Nba97GameMatchSessionRawWord replacement;
    uint32_t location_argument;
    uint32_t index;
    uint32_t record;
    unsigned i;
    TRY(validate(context, out, run));

    saved[0].word = 0;
    saved[1].word = 0;
    for (i = 0; i < 4; ++i) {
        saved[0].known[i] = 1;
        saved[1].known[i] = 1;
    }

    /* GAMEONLY 0x8002D8D4..0x8002D8FC. The fifth clear argument is a real
     * delay-slot store and the four saved registers remain live memory. */
    TRY(write_word(run, run->sp + 0x24u, 0x8002d8e8u,
        context->return_address));
    TRY(write_word(run, run->sp + 0x20u, 0x8002d8ecu, run->s[2]));
    TRY(write_word(run, run->sp + 0x1cu, 0x8002d8f0u, run->s[1]));
    TRY(write_word(run, run->sp + 0x18u, 0x8002d8f4u, run->s[0]));
    TRY(write_word(run, run->sp + 0x10u, 0x8002d8fcu, 0));
    TRY(counted_invoke(run, 0x8002d8f8u, CLEAR_RECTANGLE_ENTRY,
        NBA97_GAME_MATCH_SESSION_CLEAR_RECTANGLE, 5,
        0x200u, 0, 0x400u, 0x200u, 0,
        &out->clear_rectangle_calls, &value));

    TRY(write_half(run, 0x80021498u, 0x8002d904u, 0));
    run->s[0] = 0xf0u; /* JAL delay slot at 0x8002D90C. */
    TRY(counted_invoke(run, 0x8002d908u, FRAME_RATE_RESET_ENTRY,
        NBA97_GAME_MATCH_SESSION_FRAME_RATE_RESET, 0,
        0, 0, 0, 0, 0, &out->frame_rate_reset_calls, &value));

    run->s[2] = 0x80021eecu;
    TRY(write_word(run, run->sp + 0x10u, 0x8002d92cu, run->s[0]));
    TRY(counted_invoke(run, 0x8002d928u, SET_DEF_DRAW_ENV_ENTRY,
        NBA97_GAME_MATCH_SESSION_SET_DEF_DRAW_ENV, 5,
        run->s[2], 0, 0, 0x200u, run->s[0],
        &out->environment_calls, &value));
    run->s[1] = 0x8002205cu;
    TRY(write_word(run, run->sp + 0x10u, 0x8002d94cu, run->s[0]));
    TRY(counted_invoke(run, 0x8002d948u, SET_DEF_DISP_ENV_ENTRY,
        NBA97_GAME_MATCH_SESSION_SET_DEF_DISP_ENV, 5,
        run->s[1], 0, 0x100u, 0x200u, run->s[0],
        &out->environment_calls, &value));
    TRY(write_word(run, run->sp + 0x10u, 0x8002d964u, run->s[0]));
    TRY(counted_invoke(run, 0x8002d960u, SET_DEF_DRAW_ENV_ENTRY,
        NBA97_GAME_MATCH_SESSION_SET_DEF_DRAW_ENV, 5,
        0x80021f48u, 0, 0x100u, 0x200u, run->s[0],
        &out->environment_calls, &value));
    TRY(write_word(run, run->sp + 0x10u, 0x8002d97cu, run->s[0]));
    TRY(counted_invoke(run, 0x8002d978u, SET_DEF_DISP_ENV_ENTRY,
        NBA97_GAME_MATCH_SESSION_SET_DEF_DISP_ENV, 5,
        0x80022070u, 0, 0, 0x200u, run->s[0],
        &out->environment_calls, &value));

    /* The location word is loaded before these 13 byte stores but is not
     * tested until 0x8002D9F8. Preserve that prefix if it is unknown. */
    TRY(read_raw_word(run, CUSTOM_LOCATION_ADDRESS, 0x8002d984u, &custom));
    out->initial_custom_location = custom.word;
    run->s[1] = 0;
    TRY(write_byte(run, 0x80021f05u, 0x8002d994u, 0));
    TRY(write_byte(run, 0x80021f06u, 0x8002d99cu, 0));
    TRY(write_byte(run, 0x80021f07u, 0x8002d9a4u, 0));
    TRY(write_byte(run, 0x80021f61u, 0x8002d9acu, 0));
    TRY(write_byte(run, 0x80021f62u, 0x8002d9b4u, 0));
    TRY(write_byte(run, 0x80021f63u, 0x8002d9bcu, 0));
    TRY(write_byte(run, 0x80021f5eu, 0x8002d9c4u, 0));
    TRY(write_byte(run, 0x80021f02u, 0x8002d9ccu, 0));
    TRY(write_byte(run, 0x80021f60u, 0x8002d9d4u, 1));
    TRY(write_byte(run, 0x80021f04u, 0x8002d9dcu, 1));
    TRY(write_byte(run, 0x80022081u, 0x8002d9e4u, 0));
    TRY(write_byte(run, 0x8002206du, 0x8002d9ecu, 0));
    TRY(write_byte(run, 0x800eb680u, 0x8002d9f4u, 1));
    run->s[2] = 0; /* Branch delay slot at 0x8002D9FC. */
    TRY(require_known(run, &custom, 0x8002d9f8u,
        CUSTOM_LOCATION_ADDRESS));
    if (custom.word != 0) {
        out->initial_custom_location_active = 1;
        run->s[0] = TEAM_INDEX_ADDRESS;
        TRY(read_required_word(run, run->s[0], 0x8002da08u,
            0x8002da10u, &index));
        out->initial_team_index = index;
        record = team_record_address(index);
        out->cleared_record_address = record;
        TRY(read_raw_word(run, record, 0x8002da2cu, &saved[0]));
        TRY(read_raw_word(run, record + 4u, 0x8002da38u, &saved[1]));
        TRY(write_word(run, record, 0x8002da48u, 0));
        location_argument = custom.word & 0xffffu;
        if ((location_argument & 0x8000u) != 0)
            location_argument |= 0xffff0000u;
        TRY(counted_invoke(run, 0x8002da4cu, LOCATION_LOOKUP_ENTRY,
            NBA97_GAME_MATCH_SESSION_LOCATION_LOOKUP, 1,
            location_argument, 0, 0, 0, 0,
            &out->location_lookup_calls, &value));
        out->replacement_location = value.word;
        out->replacement_location_known = value.known;
        replacement = callback_word(&value);
        TRY(read_required_word(run, run->s[0], 0x8002da54u,
            0x8002da5cu, &index));
        out->post_lookup_team_index = index;
        record = team_record_address(index);
        out->replacement_record_address = record;
        TRY(write_raw_word(run, record + 4u, 0x8002da78u,
            &replacement));
        run->s[1] = saved[0].word;
        run->s[2] = saved[1].word;
    }
    out->saved_team_field[0] = saved[0].word;
    out->saved_team_field[1] = saved[1].word;
    out->saved_team_field_known_mask[0] = raw_known_mask(&saved[0]);
    out->saved_team_field_known_mask[1] = raw_known_mask(&saved[1]);

    TRY(counted_invoke(run, 0x8002da7cu, INITIALIZE_ENTRY,
        NBA97_GAME_MATCH_SESSION_INITIALIZE, 0,
        0, 0, 0, 0, 0, &out->session_stage_calls, &value));
    TRY(counted_invoke(run, 0x8002da84u, LOAD_SCENE_ENTRY,
        NBA97_GAME_MATCH_SESSION_LOAD_SCENE, 0,
        0, 0, 0, 0, 0, &out->session_stage_calls, &value));
    TRY(counted_invoke(run, 0x8002da8cu, RUN_LOOP_ENTRY,
        NBA97_GAME_MATCH_SESSION_RUN_LOOP, 0,
        0, 0, 0, 0, 0, &out->session_stage_calls, &value));
    TRY(counted_invoke(run, 0x8002da94u, TEARDOWN_ENTRY,
        NBA97_GAME_MATCH_SESSION_TEARDOWN, 0,
        0, 0, 0, 0, 0, &out->session_stage_calls, &value));

    TRY(read_raw_word(run, CUSTOM_LOCATION_ADDRESS, 0x8002daa0u, &custom));
    out->final_custom_location = custom.word;
    TRY(require_known(run, &custom, 0x8002daa8u,
        CUSTOM_LOCATION_ADDRESS));
    if (custom.word != 0) {
        out->final_custom_location_active = 1;
        TRY(read_required_word(run, TEAM_INDEX_ADDRESS, 0x8002dab8u,
            0x8002dac0u, &index));
        out->first_restore_team_index = index;
        record = team_record_address(index);
        out->first_restore_record_address = record;
        TRY(write_raw_word(run, record, 0x8002dadcu, &saved[0]));
        /* The retail code re-reads the index after the first store. Keep the
         * potential alias-induced split restore instead of caching it. */
        TRY(read_required_word(run, TEAM_INDEX_ADDRESS, 0x8002dae0u,
            0x8002dae8u, &index));
        out->second_restore_team_index = index;
        record = team_record_address(index);
        out->second_restore_record_address = record;
        TRY(write_raw_word(run, record + 4u, 0x8002db04u, &saved[1]));
    }

    TRY(write_byte(run, 0x80015021u, 0x8002db1cu, 0));
    TRY(write_word(run, run->sp + 0x10u, 0x8002db24u, 0));
    TRY(counted_invoke(run, 0x8002db20u, CLEAR_RECTANGLE_ENTRY,
        NBA97_GAME_MATCH_SESSION_CLEAR_RECTANGLE, 5,
        0, 0, 0x200u, 0x200u, 0,
        &out->clear_rectangle_calls, &value));

    run->s[0] = 0; /* JAL delay slot at 0x8002DB2C. */
    TRY(counted_invoke(run, 0x8002db28u, PRESENTATION_WAIT_ENTRY,
        NBA97_GAME_MATCH_SESSION_PRESENTATION_WAIT, 0,
        0, 0, 0, 0, 0, &out->presentation_wait_calls, &value));
    TRY(counted_invoke(run, 0x8002db30u, DRAW_SYNC_ENTRY,
        NBA97_GAME_MATCH_SESSION_DRAW_SYNC, 1,
        0, 0, 0, 0, 0, &out->draw_sync_calls, &value));
    do {
        ++run->s[0]; /* JAL delay slot at 0x8002DB3C. */
        TRY(counted_invoke(run, 0x8002db38u, PRESENTATION_WAIT_ENTRY,
            NBA97_GAME_MATCH_SESSION_PRESENTATION_WAIT, 0,
            0, 0, 0, 0, 0, &out->presentation_wait_calls, &value));
    } while (run->s[0] < 10u);

    /* 0x8002DB40's final SLTI leaves a known zero in v0. Child returns are
     * ignored, and the live frame controls all four restored registers. */
    out->return_v0 = 0;
    out->return_v0_known = 1;
    TRY(read_required_word(run, run->sp + 0x24u, 0x8002db4cu,
        0x8002db4cu, &out->restored_return_address));
    TRY(read_required_word(run, run->sp + 0x20u, 0x8002db50u,
        0x8002db50u, &out->restored_saved_register[2]));
    TRY(read_required_word(run, run->sp + 0x1cu, 0x8002db54u,
        0x8002db54u, &out->restored_saved_register[1]));
    TRY(read_required_word(run, run->sp + 0x18u, 0x8002db58u,
        0x8002db58u, &out->restored_saved_register[0]));
    run->sp += 0x28u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
