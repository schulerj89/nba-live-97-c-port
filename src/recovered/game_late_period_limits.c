#include "game_late_period_limits.h"

#include <string.h>

#define CLOCK_ADDRESS UINT32_C(0x800fdb58)
#define PERIOD_ADDRESS UINT32_C(0x800fdb68)
#define LIMIT_ADDRESS UINT32_C(0x8010606c)
#define HOME_ADDRESS UINT32_C(0x8001ee24)
#define AWAY_ADDRESS UINT32_C(0x8001eee8)

typedef Nba97GameLatePeriodLimitsWord Word;
typedef Nba97GameLatePeriodLimitsRegisters Registers;

typedef struct Nba97GameLatePeriodLimitsRun {
    Nba97GameLatePeriodLimitsContext* context;
    Nba97GameLatePeriodLimitsProgress* out;
    Registers registers;
} Nba97GameLatePeriodLimitsRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameLatePeriodLimitsRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameLatePeriodLimitsRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    publish(run);
}

static void constant(Word* target, uint32_t value) {
    target->word = value;
    target->known_mask = 0x0fu;
}

static uint32_t width_mask(unsigned width) {
    return width == 4u ? UINT32_MAX :
        (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}

static int registers_valid(const Registers* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0u ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameLatePeriodLimitsContext* context,
    Nba97GameLatePeriodLimitsProgress* out,
    Nba97GameLatePeriodLimitsRun* run) {
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

static int locate(Nba97GameLatePeriodLimitsRun* run, uint32_t address,
    unsigned width, uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    ++run->out->accesses;
    if (address & (uint32_t)(width - 1u))
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
                if ((*known)[j] > 1u)
                    return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}

static void journal(Nba97GameLatePeriodLimitsRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, unsigned width, const Word* value) {
    const size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameLatePeriodLimitsAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word & width_mask(width);
        event->operation = run->out->operations;
        event->width = (uint8_t)width;
        event->known_mask =
            (uint8_t)(value->known_mask & knowledge_mask(width));
        event->kind = kind;
    }
}

static int read_value(Nba97GameLatePeriodLimitsRun* run, uint32_t address,
    unsigned width, uint32_t pc, Word* value) {
    uint8_t* data;
    uint8_t* known;
    Word loaded;
    unsigned i;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, address, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_LATE_PERIOD_LIMITS_READ, pc, address, width,
        value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameLatePeriodLimitsRun* run, uint32_t address,
    unsigned width, uint32_t pc, const Word* value) {
    uint8_t* data;
    uint8_t* known;
    Word stored = *value;
    unsigned i;
    stored.word &= width_mask(width);
    stored.known_mask =
        (uint8_t)(stored.known_mask & knowledge_mask(width));
    TRY(locate(run, address, width, pc, &data, &known));
    if (!known && stored.known_mask != knowledge_mask(width))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(stored.word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_LATE_PERIOD_LIMITS_STORE, pc, address, width,
        &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static Word sign_extend_half(Word source) {
    Word result;
    const uint32_t half = source.word & UINT32_C(0xffff);
    result.word = (half & UINT32_C(0x8000)) ?
        half | UINT32_C(0xffff0000) : half;
    result.known_mask = (uint8_t)(source.known_mask & 0x03u);
    if (source.known_mask & 0x02u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Word zero_extend_half(Word source) {
    Word result;
    result.word = source.word & UINT32_C(0xffff);
    result.known_mask = (uint8_t)((source.known_mask & 0x03u) | 0x0cu);
    return result;
}

/* Retain the exact byte knowledge that survives a wrapping ADDIU carry chain. */
static Word add_constant(Word source, uint32_t addend) {
    const uint32_t original = source.word;
    const uint8_t original_known = source.known_mask;
    uint8_t result_known = 0;
    unsigned carry = 0;
    unsigned carry_known = 1;
    unsigned i;
    source.word += addend;
    for (i = 0; i < 4; ++i) {
        const unsigned byte_known = (original_known >> i) & 1u;
        const unsigned byte = (original >> (i * 8u)) & 0xffu;
        const unsigned add_byte = (addend >> (i * 8u)) & 0xffu;
        if (byte_known && carry_known)
            result_known = (uint8_t)(result_known | (uint8_t)(1u << i));
        if (byte_known && carry_known) {
            carry = byte + add_byte + carry > 0xffu;
        } else if (!byte_known && carry_known && add_byte + carry == 0u) {
            carry = 0;
            carry_known = 1;
        } else if (!byte_known && carry_known &&
            add_byte + carry == 0x100u) {
            carry = 1;
            carry_known = 1;
        } else if (byte_known && !carry_known &&
            byte + add_byte != 0xffu) {
            carry = byte + add_byte > 0xffu;
            carry_known = 1;
        } else {
            carry_known = 0;
        }
    }
    source.known_mask = result_known;
    return source;
}

static int32_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int32_t)value :
        -1 - (int32_t)~value;
}

/* Byte knownness makes every possible signed operand a finite union of
 * intervals. Global extrema are sufficient to prove or reject signed SLT. */
static void signed_extrema(Word value, int32_t* minimum, int32_t* maximum) {
    uint32_t low_min = 0;
    uint32_t low_max = 0;
    unsigned i;
    unsigned top;
    int have = 0;
    for (i = 0; i < 3; ++i) {
        const uint32_t byte = (value.word >> (i * 8u)) & 0xffu;
        if (value.known_mask & (1u << i)) {
            low_min |= byte << (i * 8u);
            low_max |= byte << (i * 8u);
        } else {
            low_max |= UINT32_C(0xff) << (i * 8u);
        }
    }
    for (top = 0; top <= 255u; ++top) {
        int32_t candidate_min;
        int32_t candidate_max;
        if ((value.known_mask & 0x08u) &&
            top != ((value.word >> 24u) & 0xffu))
            continue;
        candidate_min = signed_word(((uint32_t)top << 24u) | low_min);
        candidate_max = signed_word(((uint32_t)top << 24u) | low_max);
        if (!have || candidate_min < *minimum)
            *minimum = candidate_min;
        if (!have || candidate_max > *maximum)
            *maximum = candidate_max;
        have = 1;
    }
}

static Word slt_signed(Word left, Word right) {
    Word result;
    int32_t left_min = 0;
    int32_t left_max = 0;
    int32_t right_min = 0;
    int32_t right_max = 0;
    signed_extrema(left, &left_min, &left_max);
    signed_extrema(right, &right_min, &right_max);
    result.word = signed_word(left.word) < signed_word(right.word) ? 1u : 0u;
    result.known_mask = 0x0eu;
    if (left_max < right_min || left_min >= right_max)
        result.known_mask = 0x0fu;
    return result;
}

static Word slti_signed(Word left, int32_t immediate) {
    Word right;
    right.word = (uint32_t)immediate;
    right.known_mask = 0x0fu;
    return slt_signed(left, right);
}

static int decide_zero(const Word* value, int* is_zero) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (i * 8u)) & 0xffu)) {
            *is_zero = 0;
            return NBA97_TEXT_COMPLETE;
        }
    if (value->known_mask == 0x0fu) {
        *is_zero = 1;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_UNKNOWN;
}

int nba97_game_late_period_limits(Nba97GameLatePeriodLimitsContext* context,
    Nba97GameLatePeriodLimitsProgress* out) {
    Nba97GameLatePeriodLimitsRun storage;
    Nba97GameLatePeriodLimitsRun* run = &storage;
    Word value;
    Word branch_value;
    int is_zero;
    int decision;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80067550..0x80067568: LW feeds signed SLTI. The zero store
     * executes in BEQ's delay slot before an unknown predicate can stop. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, CLOCK_ADDRESS, 4, UINT32_C(0x80067554), &value));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = value;
    out->clock = value;
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
    REG(run, NBA97_MATCH_INITIALIZE_A0) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x0000606c));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = slti_signed(
        REG(run, NBA97_MATCH_INITIALIZE_V0), 0x1c20);
    branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
    constant(&value, 0);
    TRY(write_value(run, LIMIT_ADDRESS, 2, UINT32_C(0x80067568), &value));
    decision = decide_zero(&branch_value, &is_zero);
    if (decision != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x80067564), 0);
        return decision;
    }
    if (is_zero)
        goto source_return;

    /* GAMEONLY 0x8006756C..0x80067580: LH sign-extends period. The second
     * SLTI is the BNE delay and therefore replaces v0 even on the early arm. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, PERIOD_ADDRESS, 2, UINT32_C(0x80067570), &value));
    REG(run, NBA97_MATCH_INITIALIZE_V1) = sign_extend_half(value);
    out->period = REG(run, NBA97_MATCH_INITIALIZE_V1);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = slti_signed(
        REG(run, NBA97_MATCH_INITIALIZE_V1), 3);
    branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = slti_signed(
        REG(run, NBA97_MATCH_INITIALIZE_V1), 4);
    decision = decide_zero(&branch_value, &is_zero);
    if (decision != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x8006757c), 0);
        return decision;
    }
    if (!is_zero)
        goto source_return;

    /* GAMEONLY 0x80067584..0x80067590: the BNE delay chooses five first;
     * periods other than exactly three replace it with four before SH. */
    branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V0), 5);
    decision = decide_zero(&branch_value, &is_zero);
    if (decision != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x80067584), 0);
        return decision;
    }
    if (is_zero)
        constant(&REG(run, NBA97_MATCH_INITIALIZE_V0), 4);
    TRY(write_value(run, LIMIT_ADDRESS, 2, UINT32_C(0x80067590),
        &REG(run, NBA97_MATCH_INITIALIZE_V0)));
    out->selected_late_period_limit = 1;

    /* GAMEONLY 0x80067594..0x800675B8: reload the just-published limit live,
     * compare unsigned home data as signed zero-extended data, and execute
     * v0=v1-2 in BEQ's delay before the optional home store. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020000));
    REG(run, NBA97_MATCH_INITIALIZE_A1) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_A1), UINT32_C(0xffffee24));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, LIMIT_ADDRESS, 2, UINT32_C(0x800675a0), &value));
    REG(run, NBA97_MATCH_INITIALIZE_V1) = zero_extend_half(value);
    out->limit = REG(run, NBA97_MATCH_INITIALIZE_V1);
    TRY(read_value(run, HOME_ADDRESS, 2, UINT32_C(0x800675a4), &value));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = zero_extend_half(value);
    out->home_before = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_A0) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffffe));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = slt_signed(
        REG(run, NBA97_MATCH_INITIALIZE_V0),
        REG(run, NBA97_MATCH_INITIALIZE_A0));
    branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffffe));
    decision = decide_zero(&branch_value, &is_zero);
    if (decision != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x800675b0), 0);
        return decision;
    }
    if (!is_zero) {
        TRY(write_value(run, HOME_ADDRESS, 2, UINT32_C(0x800675b8),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        out->home_raised = 1;
    }

    /* GAMEONLY 0x800675BC..0x800675D8: LHU reloads away after the optional
     * home store, preserving native-storage aliases and the second BEQ delay. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_value(run, AWAY_ADDRESS, 2, UINT32_C(0x800675c0), &value));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = zero_extend_half(value);
    out->away_before = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = slt_signed(
        REG(run, NBA97_MATCH_INITIALIZE_V0),
        REG(run, NBA97_MATCH_INITIALIZE_A0));
    branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_V0) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffffe));
    decision = decide_zero(&branch_value, &is_zero);
    if (decision != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x800675cc), 0);
        return decision;
    }
    if (!is_zero) {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
        TRY(write_value(run, AWAY_ADDRESS, 2, UINT32_C(0x800675d8),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        out->away_raised = 1;
    }

source_return:
    /* 0x800675DC JR ra; 0x800675E0 NOP. All stores precede consumption of
     * the untouched leaf return register. */
    if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x800675dc), 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
