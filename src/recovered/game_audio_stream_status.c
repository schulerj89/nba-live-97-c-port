#include "game_audio_stream_status.h"

#include <string.h>

#define FLAGS_ADDRESS UINT32_C(0x800c43b0)
#define BUSY_ADDRESS UINT32_C(0x800c43b1)

typedef struct Nba97GameAudioStreamStatusRun {
    Nba97GameAudioStreamStatusContext* context;
    Nba97GameAudioStreamStatusProgress* out;
    Nba97GameAudioStreamStatusRegisters registers;
} Nba97GameAudioStreamStatusRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameAudioStreamStatusRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameAudioStreamStatusRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    publish(run);
}

static int spend(Nba97GameAudioStreamStatusRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(
    const Nba97GameAudioStreamStatusRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameAudioStreamStatusContext* context,
    Nba97GameAudioStreamStatusProgress* out,
    Nba97GameAudioStreamStatusRun* run) {
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

/* ADDIU uses a byte carry chain. Unknown source bytes range over all 256
 * values; a result byte is known only when every possible path agrees. */
static Nba97GameAudioStreamStatusWord add_constant(
    Nba97GameAudioStreamStatusWord input, uint32_t constant) {
    Nba97GameAudioStreamStatusWord result;
    unsigned byte;
    unsigned carry_mask = 1u;
    result.word = input.word + constant;
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        unsigned next_carry_mask = 0;
        unsigned first_output = 0;
        int first = 1;
        int invariant = 1;
        unsigned carry;
        unsigned start = (input.known_mask & (1u << byte)) ?
            ((input.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned end = (input.known_mask & (1u << byte)) ? start : 255u;
        unsigned addend = (constant >> (byte * 8u)) & 0xffu;
        for (carry = 0; carry <= 1; ++carry) {
            unsigned source;
            if (!(carry_mask & (1u << carry)))
                continue;
            for (source = start; source <= end; ++source) {
                unsigned sum = source + addend + carry;
                unsigned output = sum & 0xffu;
                next_carry_mask |= 1u << (sum >> 8u);
                if (first) {
                    first_output = output;
                    first = 0;
                } else if (output != first_output) {
                    invariant = 0;
                }
            }
        }
        if (invariant)
            result.known_mask =
                (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
        carry_mask = next_carry_mask;
    }
    return result;
}

static Nba97GameAudioStreamStatusWord and_immediate(
    Nba97GameAudioStreamStatusWord source, uint16_t immediate) {
    Nba97GameAudioStreamStatusWord result;
    unsigned byte;
    result.word = source.word & immediate;
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        unsigned immediate_byte = (immediate >> (byte * 8u)) & 0xffu;
        if (!immediate_byte || (source.known_mask & (1u << byte)))
            result.known_mask =
                (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    }
    return result;
}

static void set_known(Nba97GameAudioStreamStatusWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static void journal(Nba97GameAudioStreamStatusRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameAudioStreamStatusWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameAudioStreamStatusAccess* event =
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

static int locate(Nba97GameAudioStreamStatusRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address);
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

static int read_byte(Nba97GameAudioStreamStatusRun* run, uint32_t address,
    uint32_t pc, Nba97GameAudioStreamStatusWord* value) {
    uint8_t* data;
    uint8_t* known;
    TRY(locate(run, address, 1, 1, pc, &data, &known));
    value->word = *data;
    value->known_mask = (uint8_t)(0x0eu | ((!known || *known) ? 1u : 0u));
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_STREAM_STATUS_READ, pc, address, 1, value);
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameAudioStreamStatusRun* run, uint32_t address,
    uint32_t pc, Nba97GameAudioStreamStatusWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameAudioStreamStatusWord loaded = {0, 0};
    unsigned byte;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (byte = 0; byte < 4; ++byte) {
        loaded.word |= (uint32_t)data[byte] << (byte * 8u);
        if (!known || known[byte])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_STREAM_STATUS_READ, pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameAudioStreamStatusRun* run, uint32_t address,
    uint32_t pc, const Nba97GameAudioStreamStatusWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned byte;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (byte = 0; byte < 4; ++byte) {
        data[byte] = (uint8_t)(value->word >> (byte * 8u));
        if (known)
            known[byte] = (uint8_t)((value->known_mask >> byte) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_AUDIO_STREAM_STATUS_STORE, pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int effective_address(Nba97GameAudioStreamStatusRun* run,
    unsigned base_register, uint32_t offset, uint32_t pc,
    uint32_t* address) {
    Nba97GameAudioStreamStatusWord computed =
        add_constant(REG(run, base_register), offset);
    if (computed.known_mask != 0x0fu) {
        stop(run, pc, computed.word);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = computed.word;
    return NBA97_TEXT_COMPLETE;
}

static int decide_zero(Nba97GameAudioStreamStatusRun* run,
    const Nba97GameAudioStreamStatusWord* value, uint32_t pc,
    int* is_zero) {
    unsigned byte;
    for (byte = 0; byte < 4; ++byte)
        if ((value->known_mask & (1u << byte)) &&
            ((value->word >> (byte * 8u)) & 0xffu)) {
            *is_zero = 0;
            return NBA97_TEXT_COMPLETE;
        }
    if (value->known_mask == 0x0fu) {
        *is_zero = 1;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int finish(Nba97GameAudioStreamStatusRun* run) {
    uint32_t address;
    run->out->returned_value = REG(run, NBA97_MATCH_INITIALIZE_V0);

    /* GAMEONLY 0x800847FC..0x8008480C: restore sp from live s8, reload saved
     * s8, add eight with exact carry knownness, and consume unchanged ra at
     * JR before its NOP delay. */
    REG(run, NBA97_MATCH_INITIALIZE_SP) = REG(run, NBA97_MATCH_INITIALIZE_FP);
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0,
        UINT32_C(0x80084800), &address));
    TRY(read_word(run, address, UINT32_C(0x80084800),
        &REG(run, NBA97_MATCH_INITIALIZE_FP)));
    run->out->restored_s8 = REG(run, NBA97_MATCH_INITIALIZE_FP);
    REG(run, NBA97_MATCH_INITIALIZE_SP) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_SP), 8u);
    publish(run);
    if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80084808), 0);
        return NBA97_TEXT_UNKNOWN;
    }
    /* JR 0x80084808 executes the NOP delay at 0x8008480C. */
    run->out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_audio_stream_status(Nba97GameAudioStreamStatusContext* context,
    Nba97GameAudioStreamStatusProgress* out) {
    Nba97GameAudioStreamStatusRun storage;
    Nba97GameAudioStreamStatusRun* run = &storage;
    Nba97GameAudioStreamStatusWord loaded;
    uint32_t address;
    int is_zero;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8008472C..0x80084734: allocate the eight-byte frame, save
     * the complete per-byte-known s8 word, then establish s8 as frame base. */
    REG(run, NBA97_MATCH_INITIALIZE_SP) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xfffffff8));
    out->frame_stack_pointer = REG(run, NBA97_MATCH_INITIALIZE_SP).word;
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0,
        UINT32_C(0x80084730), &address));
    TRY(write_word(run, address, UINT32_C(0x80084730),
        &REG(run, NBA97_MATCH_INITIALIZE_FP)));
    REG(run, NBA97_MATCH_INITIALIZE_FP) = REG(run, NBA97_MATCH_INITIALIZE_SP);

    /* 0x80084738..0x80084750: LUI and the first live LBU are followed by a
     * source NOP. Both ANDIs force known-zero upper bytes before BNE. */
    set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_byte(run, FLAGS_ADDRESS, UINT32_C(0x8008473c), &loaded));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = loaded;
    out->first_flags = loaded;
    REG(run, NBA97_MATCH_INITIALIZE_V1) = and_immediate(
        REG(run, NBA97_MATCH_INITIALIZE_V0), 2u);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = and_immediate(
        REG(run, NBA97_MATCH_INITIALIZE_V1), 0xffu);
    publish(run);
    TRY(decide_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x8008474c), &is_zero));
    if (is_zero) {
        /* 0x80084754..0x8008475C: signed -14 then J/NOP to epilogue. */
        set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xfffffff2));
        return finish(run);
    }

    /* Static full-span annotation only: raw words 0x080211FF/0x00000000 at
     * 0x80084760/64 are an unreachable redundant J 0x800847FC/NOP pair and
     * are excluded from the 49-instruction Ghidra function body. */

    /* 0x80084768..0x80084778: the busy byte is loaded after a fresh LUI and
     * its own NOP; BEQ consumes the zero-extended live byte. */
    set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_byte(run, BUSY_ADDRESS, UINT32_C(0x8008476c), &loaded));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = loaded;
    out->busy = loaded;
    publish(run);
    TRY(decide_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80084774), &is_zero));
    if (!is_zero) {
        /* 0x8008477C..0x80084784: ORI v0,zero,4 then J/NOP. */
        set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), 4u);
        return finish(run);
    }

    /* Static full-span annotation only: the unreachable 0x80084788/8C pair
     * repeats J 0x800847FC/NOP and is not executed by this owner. */

    /* 0x80084790..0x800847A8: reload flags rather than reusing the first
     * byte, preserve the LBU NOP, and test bit 0 through two ANDIs. */
    set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_byte(run, FLAGS_ADDRESS, UINT32_C(0x80084794), &loaded));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = loaded;
    out->second_flags = loaded;
    REG(run, NBA97_MATCH_INITIALIZE_V1) = and_immediate(
        REG(run, NBA97_MATCH_INITIALIZE_V0), 1u);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = and_immediate(
        REG(run, NBA97_MATCH_INITIALIZE_V1), 0xffu);
    publish(run);
    TRY(decide_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x800847a4), &is_zero));
    if (is_zero) {
        /* 0x800847AC..0x800847B4: ORI v0,zero,1 then J/NOP. */
        set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), 1u);
        return finish(run);
    }

    /* Static full-span annotation only: the unreachable 0x800847B8/BC pair
     * repeats J 0x800847FC/NOP outside the executable body. */

    /* 0x800847C0..0x800847D8: a third distinct flags load tests bit 2 and
     * retains the source NOP and known-zero upper bytes from both ANDIs. */
    set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_byte(run, FLAGS_ADDRESS, UINT32_C(0x800847c4), &loaded));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = loaded;
    out->third_flags = loaded;
    REG(run, NBA97_MATCH_INITIALIZE_V1) = and_immediate(
        REG(run, NBA97_MATCH_INITIALIZE_V0), 4u);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = and_immediate(
        REG(run, NBA97_MATCH_INITIALIZE_V1), 0xffu);
    publish(run);
    TRY(decide_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x800847d4), &is_zero));
    if (!is_zero) {
        /* 0x800847DC..0x800847E4: ORI v0,zero,3 then J/NOP. */
        set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), 3u);
        return finish(run);
    }

    /* Static full-span annotation only: the unreachable 0x800847E8/EC pair
     * repeats J 0x800847FC/NOP. Executable 0x800847F0..F8 sets v0=1 and
     * reaches the common epilogue through its own J/NOP. */
    set_known(&REG(run, NBA97_MATCH_INITIALIZE_V0), 1u);
    return finish(run);
}
