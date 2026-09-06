#include "game_audio_stream_pump.h"

#include <limits.h>
#include <string.h>

typedef struct Nba97GameAudioStreamPumpRun {
    Nba97GameAudioStreamPumpContext* context;
    Nba97GameAudioStreamPumpProgress* out;
    Nba97GameAudioStreamPumpRegisters registers;
} Nba97GameAudioStreamPumpRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameAudioStreamPumpRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameAudioStreamPumpRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int registers_valid(const Nba97GameAudioStreamPumpRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameAudioStreamPumpContext* context,
    Nba97GameAudioStreamPumpProgress* out,
    Nba97GameAudioStreamPumpRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count) ||
        (!context->access_journal && context->access_journal_capacity) ||
        !registers_valid(&context->registers))
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
    run->registers = context->registers;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static void set_known(Nba97GameAudioStreamPumpWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static Nba97GameAudioStreamPumpWord add_constant(
    Nba97GameAudioStreamPumpWord source, uint32_t addend) {
    const uint32_t original = source.word;
    const uint8_t original_known = source.known_mask;
    uint8_t result_known = 0;
    unsigned carry = 0;
    unsigned carry_known = 1;
    unsigned i;
    source.word += addend;
    for (i = 0; i < 4; ++i) {
        const unsigned byte_known = (original_known >> i) & 1u;
        const unsigned byte = (original >> (8u * i)) & 0xffu;
        const unsigned add_byte = (addend >> (8u * i)) & 0xffu;
        if (byte_known && carry_known)
            result_known = (uint8_t)(result_known | (1u << i));
        if (byte_known && carry_known) {
            carry = byte + add_byte + carry > 0xffu;
        } else if (!byte_known && carry_known && add_byte + carry == 0u) {
            carry = 0;
        } else if (!byte_known && carry_known && add_byte + carry == 0x100u) {
            carry = 1;
        } else if (byte_known && !carry_known && byte + add_byte != 0xffu) {
            carry = byte + add_byte > 0xffu;
            carry_known = 1;
        } else {
            carry_known = 0;
        }
    }
    source.known_mask = result_known;
    return source;
}

static Nba97GameAudioStreamPumpWord and_immediate(
    Nba97GameAudioStreamPumpWord source, uint16_t immediate) {
    Nba97GameAudioStreamPumpWord result;
    unsigned i;
    result.word = source.word & immediate;
    result.known_mask = 0;
    for (i = 0; i < 4; ++i) {
        const unsigned immediate_byte = (immediate >> (8u * i)) & 0xffu;
        if (!immediate_byte || (source.known_mask & (1u << i)))
            result.known_mask = (uint8_t)(result.known_mask | (1u << i));
    }
    return result;
}

static int spend(Nba97GameAudioStreamPumpRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameAudioStreamPumpRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameAudioStreamPumpWord* value) {
    const size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameAudioStreamPumpAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word;
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask = value->known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameAudioStreamPumpRun* run, uint32_t address,
    uint32_t pc, size_t width, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (width == 4 && (address & 3u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        const uint64_t offset = (uint64_t)address - region->base;
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

static int read_word(Nba97GameAudioStreamPumpRun* run, uint32_t address,
    uint32_t pc, Nba97GameAudioStreamPumpWord* value) {
    Nba97GameAudioStreamPumpWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, 4, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (8u * i);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_STREAM_PUMP_READ, pc, address, 4, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int read_byte_unsigned(Nba97GameAudioStreamPumpRun* run,
    uint32_t address, uint32_t pc, Nba97GameAudioStreamPumpWord* value) {
    uint8_t* data;
    uint8_t* known;
    TRY(locate(run, address, pc, 1, &data, &known));
    value->word = *data;
    value->known_mask = (uint8_t)(0x0eu | ((!known || *known) ? 1u : 0u));
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_STREAM_PUMP_READ, pc, address, 1, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameAudioStreamPumpRun* run, uint32_t address,
    uint32_t pc, const Nba97GameAudioStreamPumpWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, 4, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (8u * i));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_AUDIO_STREAM_PUMP_STORE, pc, address, 4, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int require_known(Nba97GameAudioStreamPumpRun* run,
    const Nba97GameAudioStreamPumpWord* value, uint32_t pc) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, value->word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int base_read(Nba97GameAudioStreamPumpRun* run, unsigned base,
    uint32_t offset, uint32_t pc, Nba97GameAudioStreamPumpWord* value) {
    Nba97GameAudioStreamPumpWord address = add_constant(R(base), offset);
    TRY(require_known(run, &address, pc));
    return read_word(run, address.word, pc, value);
}

static int base_write(Nba97GameAudioStreamPumpRun* run, unsigned base,
    uint32_t offset, uint32_t pc, const Nba97GameAudioStreamPumpWord* value) {
    Nba97GameAudioStreamPumpWord address = add_constant(R(base), offset);
    TRY(require_known(run, &address, pc));
    return write_word(run, address.word, pc, value);
}

static int invoke(Nba97GameAudioStreamPumpRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count) {
    Nba97GameAudioStreamPumpEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[kind] + 1u;
    event.kind = kind;
    event.argument_count = argument_count;
    publish(run);
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user, &run->context->memory,
        &event, &run->registers);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!registers_valid(&run->registers))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[kind];
    return NBA97_TEXT_COMPLETE;
}

static int zero_state(const Nba97GameAudioStreamPumpWord* value) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (8u * i)) & 0xffu) != 0)
            return 0;
    return value->known_mask == 0x0fu ? 1 : -1;
}

static int equality(const Nba97GameAudioStreamPumpWord* left,
    const Nba97GameAudioStreamPumpWord* right, int* equal) {
    unsigned i;
    for (i = 0; i < 4; ++i) {
        const uint8_t bit = (uint8_t)(1u << i);
        if ((left->known_mask & bit) && (right->known_mask & bit) &&
            ((left->word >> (8u * i)) & 0xffu) !=
            ((right->word >> (8u * i)) & 0xffu)) {
            *equal = 0;
            return 1;
        }
    }
    if ((left->known_mask & right->known_mask) == 0x0fu) {
        *equal = left->word == right->word;
        return 1;
    }
    return 0;
}

static void signed_bounds(const Nba97GameAudioStreamPumpWord* value,
    int64_t* minimum, int64_t* maximum) {
    unsigned i;
    uint32_t low = 0;
    uint32_t high = 0;
    if (!(value->known_mask & 8u)) {
        *minimum = INT32_MIN;
        *maximum = INT32_MAX;
        return;
    }
    for (i = 0; i < 4; ++i) {
        const uint32_t byte = (value->word >> (8u * i)) & 0xffu;
        low |= ((value->known_mask & (1u << i)) ? byte : 0u) << (8u * i);
        high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) << (8u * i);
    }
    *minimum = low < UINT32_C(0x80000000) ? (int64_t)low :
        (int64_t)low - INT64_C(0x100000000);
    *maximum = high < UINT32_C(0x80000000) ? (int64_t)high :
        (int64_t)high - INT64_C(0x100000000);
}

static int64_t signed_word(uint32_t word) {
    return word < UINT32_C(0x80000000) ? (int64_t)word :
        (int64_t)word - INT64_C(0x100000000);
}

/* Return one when the signed predicate is known, with relation selected as
 * -1 (< constant), 0 (<= constant), 1 (> constant), or 2 (>= constant). */
static int signed_predicate(const Nba97GameAudioStreamPumpWord* value,
    int64_t constant, int relation, int* taken) {
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    if (relation == -1) {
        if (maximum < constant) { *taken = 1; return 1; }
        if (minimum >= constant) { *taken = 0; return 1; }
    } else if (relation == 0) {
        if (maximum <= constant) { *taken = 1; return 1; }
        if (minimum > constant) { *taken = 0; return 1; }
    } else if (relation == 1) {
        if (minimum > constant) { *taken = 1; return 1; }
        if (maximum <= constant) { *taken = 0; return 1; }
    } else {
        if (minimum >= constant) { *taken = 1; return 1; }
        if (maximum < constant) { *taken = 0; return 1; }
    }
    *taken = signed_word(value->word) < constant;
    return 0;
}

static int slti(Nba97GameAudioStreamPumpWord source, int32_t immediate,
    Nba97GameAudioStreamPumpWord* result) {
    int taken;
    const int known = signed_predicate(&source, immediate, -1, &taken);
    result->word = signed_word(source.word) < immediate;
    result->known_mask = 0x0eu;
    if (known)
        result->known_mask = 0x0fu;
    return known;
}

static int unknown_branch(Nba97GameAudioStreamPumpRun* run, uint32_t pc) {
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

int nba97_game_audio_stream_pump(Nba97GameAudioStreamPumpContext* context,
    Nba97GameAudioStreamPumpProgress* out) {
    Nba97GameAudioStreamPumpRun storage;
    Nba97GameAudioStreamPumpRun* run = &storage;
    Nba97GameAudioStreamPumpWord zero;
    int branch_known;
    int branch_taken;
    int first_positive;
    TRY(validate(context, out, run));
    set_known(&zero, 0);

    /* GAMEONLY 0x80083EEC..0x80083F04: form the frame, save ra/s8,
     * establish live s8, initialize the return slot, and call the gate. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_SP, 0x1cu,
        UINT32_C(0x80083ef0), &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_SP, 0x18u,
        UINT32_C(0x80083ef4), &R(NBA97_MATCH_INITIALIZE_FP)));
    R(NBA97_MATCH_INITIALIZE_FP) = R(NBA97_MATCH_INITIALIZE_SP);
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_FP, 0x14u,
        UINT32_C(0x80083efc), &zero));
    TRY(invoke(run, UINT32_C(0x80083f00), UINT32_C(0x8008472c),
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C, 0));
    out->initial_status = R(NBA97_MATCH_INITIALIZE_V0);

    /* 0x80083F08..0x80083F18: BGEZ has a NOP delay. The negative bypass
     * clears v0 and skips the live return-slot load. */
    branch_known = signed_predicate(&R(NBA97_MATCH_INITIALIZE_V0), 0, 2,
        &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80083f08));
    if (!branch_taken) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0);
        goto epilogue;
    }

    /* 0x80083F1C..0x80083F38: load the raw flag byte and preserve both
     * ANDIs, including their known-zero upper bytes, before mode dispatch. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_byte_unsigned(run, UINT32_C(0x800c43b0),
        UINT32_C(0x80083f20), &R(NBA97_MATCH_INITIALIZE_V0)));
    out->first_flags = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V1) = and_immediate(
        R(NBA97_MATCH_INITIALIZE_V0), UINT16_C(0x0005));
    R(NBA97_MATCH_INITIALIZE_V0) = and_immediate(
        R(NBA97_MATCH_INITIALIZE_V1), UINT16_C(0x00ff));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), 4);
    branch_known = equality(&R(NBA97_MATCH_INITIALIZE_V0),
        &R(NBA97_MATCH_INITIALIZE_V1), &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80083f34));
    if (branch_taken)
        goto mode4;

    /* 0x80083F3C..0x80083F74: retain the exact signed SLTI/branch tree.
     * The BLTZ arm is source-reachable code even though ANDI makes a known
     * mode nonnegative. */
    slti(R(NBA97_MATCH_INITIALIZE_V0), 5,
        &R(NBA97_MATCH_INITIALIZE_V1));
    branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V1));
    if (branch_taken < 0)
        return unknown_branch(run, UINT32_C(0x80083f40));
    if (branch_taken) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), 5);
        branch_known = equality(&R(NBA97_MATCH_INITIALIZE_V0),
            &R(NBA97_MATCH_INITIALIZE_V1), &branch_taken);
        if (!branch_known)
            return unknown_branch(run, UINT32_C(0x80083f68));
        if (branch_taken)
            goto mode5_loop;
        goto common_return;
    }
    slti(R(NBA97_MATCH_INITIALIZE_V0), 2,
        &R(NBA97_MATCH_INITIALIZE_V1));
    branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V1));
    if (branch_taken < 0)
        return unknown_branch(run, UINT32_C(0x80083f4c));
    if (branch_taken)
        goto common_return;
    branch_known = signed_predicate(&R(NBA97_MATCH_INITIALIZE_V0), 0, -1,
        &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80083f54));
    goto common_return;

mode5_loop:
    /* 0x80083F78..0x80083F90: service, reload the live global handle,
     * query status, and store its raw v0 through callback-live s8. */
    TRY(invoke(run, UINT32_C(0x80083f78), UINT32_C(0x80086190),
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190, 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800c0000));
    TRY(read_word(run, UINT32_C(0x800c438c), UINT32_C(0x80083f84),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    TRY(invoke(run, UINT32_C(0x80083f88), UINT32_C(0x80088018),
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018, 1));
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
        UINT32_C(0x80083f90), &R(NBA97_MATCH_INITIALIZE_V0)));

    /* 0x80083F94..0x80083FD8: first test signed >0; only its false path
     * performs the second live reload and signed SLTI -9. */
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
        UINT32_C(0x80083f94), &R(NBA97_MATCH_INITIALIZE_V0)));
    out->last_stream_status = R(NBA97_MATCH_INITIALIZE_V0);
    branch_known = signed_predicate(&R(NBA97_MATCH_INITIALIZE_V0), 0, 1,
        &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80083f9c));
    if (!branch_taken) {
        TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
            UINT32_C(0x80083fa4), &R(NBA97_MATCH_INITIALIZE_V0)));
        slti(R(NBA97_MATCH_INITIALIZE_V0), -9,
            &R(NBA97_MATCH_INITIALIZE_V1));
        branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V1));
        if (branch_taken < 0)
            return unknown_branch(run, UINT32_C(0x80083fb0));
        branch_taken = !branch_taken;
    }
    if (branch_taken) {
        TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
            UINT32_C(0x80083fc0), &R(NBA97_MATCH_INITIALIZE_A0)));
        TRY(invoke(run, UINT32_C(0x80083fc4), UINT32_C(0x800840f0),
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0, 1));
        TRY(base_write(run, NBA97_MATCH_INITIALIZE_FP, 0x14u,
            UINT32_C(0x80083fcc), &R(NBA97_MATCH_INITIALIZE_V0)));
    } else {
        TRY(base_write(run, NBA97_MATCH_INITIALIZE_FP, 0x14u,
            UINT32_C(0x80083fd8), &zero));
    }

    /* 0x80083FDC..0x8008400C: the loop predicate repeats the two separate
     * status loads and the signed >0 OR < -9 tests in source order. */
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
        UINT32_C(0x80083fdc), &R(NBA97_MATCH_INITIALIZE_V0)));
    out->last_stream_status = R(NBA97_MATCH_INITIALIZE_V0);
    branch_known = signed_predicate(&R(NBA97_MATCH_INITIALIZE_V0), 0, 1,
        &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80083fe4));
    if (!branch_taken) {
        TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
            UINT32_C(0x80083fec), &R(NBA97_MATCH_INITIALIZE_V0)));
        slti(R(NBA97_MATCH_INITIALIZE_V0), -9,
            &R(NBA97_MATCH_INITIALIZE_V1));
        branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V1));
        if (branch_taken < 0)
            return unknown_branch(run, UINT32_C(0x80083ff8));
        branch_taken = !branch_taken;
    }
    if (branch_taken)
        goto mode5_loop;
    goto common_return;

mode4:
    /* 0x80084018..0x80084030: mode 4 deliberately reloads the flag byte;
     * only its live bit 1 controls whether the loop is entered. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_byte_unsigned(run, UINT32_C(0x800c43b0),
        UINT32_C(0x8008401c), &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V1) = and_immediate(
        R(NBA97_MATCH_INITIALIZE_V0), UINT16_C(0x0002));
    R(NBA97_MATCH_INITIALIZE_V0) = and_immediate(
        R(NBA97_MATCH_INITIALIZE_V1), UINT16_C(0x00ff));
    branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V0));
    if (branch_taken < 0)
        return unknown_branch(run, UINT32_C(0x8008402c));
    if (branch_taken)
        goto common_return;

mode4_loop:
    /* 0x80084034..0x8008404C: query and retain the raw status exactly as
     * in mode 5, with both children free to replace s8. */
    TRY(invoke(run, UINT32_C(0x80084034), UINT32_C(0x80086190),
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190, 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800c0000));
    TRY(read_word(run, UINT32_C(0x800c438c), UINT32_C(0x80084040),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    TRY(invoke(run, UINT32_C(0x80084044), UINT32_C(0x80088018),
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018, 1));
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
        UINT32_C(0x8008404c), &R(NBA97_MATCH_INITIALIZE_V0)));

    /* 0x80084050..0x80084084: preserve the original contradictory AND:
     * first reload must be >0, then the second reload must be < -9. */
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
        UINT32_C(0x80084050), &R(NBA97_MATCH_INITIALIZE_V0)));
    out->last_stream_status = R(NBA97_MATCH_INITIALIZE_V0);
    branch_known = signed_predicate(&R(NBA97_MATCH_INITIALIZE_V0), 0, 0,
        &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80084058));
    first_positive = !branch_taken;
    if (first_positive) {
        TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
            UINT32_C(0x80084060), &R(NBA97_MATCH_INITIALIZE_V0)));
        slti(R(NBA97_MATCH_INITIALIZE_V0), -9,
            &R(NBA97_MATCH_INITIALIZE_V1));
        branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V1));
        if (branch_taken < 0)
            return unknown_branch(run, UINT32_C(0x8008406c));
        if (!branch_taken) {
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800c0000));
            TRY(read_word(run, UINT32_C(0x800c438c),
                UINT32_C(0x80084078), &R(NBA97_MATCH_INITIALIZE_A0)));
            TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
                UINT32_C(0x8008407c), &R(NBA97_MATCH_INITIALIZE_A1)));
            TRY(invoke(run, UINT32_C(0x80084080), UINT32_C(0x80088288),
                NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088288, 2));
        }
    }

    /* 0x80084088..0x800840B8: handler return is unused. The live status
     * slot is loaded once for >0 and again for < -9 before looping. */
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
        UINT32_C(0x80084088), &R(NBA97_MATCH_INITIALIZE_V0)));
    out->last_stream_status = R(NBA97_MATCH_INITIALIZE_V0);
    branch_known = signed_predicate(&R(NBA97_MATCH_INITIALIZE_V0), 0, 1,
        &branch_taken);
    if (!branch_known)
        return unknown_branch(run, UINT32_C(0x80084090));
    if (!branch_taken) {
        TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x10u,
            UINT32_C(0x80084098), &R(NBA97_MATCH_INITIALIZE_V0)));
        slti(R(NBA97_MATCH_INITIALIZE_V0), -9,
            &R(NBA97_MATCH_INITIALIZE_V1));
        branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V1));
        if (branch_taken < 0)
            return unknown_branch(run, UINT32_C(0x800840a4));
        branch_taken = !branch_taken;
    }
    if (branch_taken)
        goto mode4_loop;
    goto common_return;

common_return:
    /* 0x800840CC..0x800840D4: every nonnegative route returns the live
     * frame+0x14 word, including values written by aliased child memory. */
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_FP, 0x14u,
        UINT32_C(0x800840cc), &R(NBA97_MATCH_INITIALIZE_V0)));

epilogue:
    /* 0x800840D8..0x800840EC: move sp from callback-live s8, reload ra/s8,
     * advance that sp with 32-bit carry knownness, and consume live ra. */
    R(NBA97_MATCH_INITIALIZE_SP) = R(NBA97_MATCH_INITIALIZE_FP);
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_SP, 0x1cu,
        UINT32_C(0x800840dc), &R(NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_SP, 0x18u,
        UINT32_C(0x800840e0), &R(NBA97_MATCH_INITIALIZE_FP)));
    out->restored_s8 = R(NBA97_MATCH_INITIALIZE_FP);
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    out->returned_value = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(require_known(run, &R(NBA97_MATCH_INITIALIZE_RA),
        UINT32_C(0x800840e8)));
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
