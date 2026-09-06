#include "game_match_clocks.h"

#include <limits.h>
#include <string.h>

#define MAIN_CLOCK UINT32_C(0x800fdb58)
#define PHASE UINT32_C(0x800fdb90)
#define PHASE_82_BLOCK UINT32_C(0x800fe882)
#define PHASE_82_ENABLE UINT32_C(0x80021d90)
#define PHASE_82_LIMIT UINT32_C(0x800fdb5c)
#define PHASE_82_STOP UINT32_C(0x800fdb60)
#define SHOT_CLOCK UINT32_C(0x800fdba4)
#define SHOT_SOUND_ENABLE UINT32_C(0x80021d92)
#define TEAM_SIGNAL UINT32_C(0x800fdb86)
#define HOME_TIMER UINT32_C(0x8001eeb4)
#define HOME_STATE UINT32_C(0x8001eeb6)
#define AWAY_TIMER UINT32_C(0x8001ef78)
#define AWAY_STATE UINT32_C(0x8001ef7a)

typedef struct Nba97GameMatchClocksRun {
    Nba97GameMatchClocksContext* context;
    Nba97GameMatchClocksProgress* out;
    Nba97GameMatchClocksMachine machine;
} Nba97GameMatchClocksRun;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameMatchClocksRun* run) {
    run->out->machine = run->machine;
}

static void stop(Nba97GameMatchClocksRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameMatchClocksWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameMatchClocksMachine* machine) {
    unsigned i;
    if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
        machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine->registers.gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameMatchClocksContext* context,
    Nba97GameMatchClocksProgress* out, Nba97GameMatchClocksRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count) ||
        (!context->access_journal && context->access_journal_capacity) ||
        !machine_valid(&context->machine))
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
    run->machine = context->machine;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

/* Byte-domain carry enumeration keeps exactly the bytes invariant for every
 * concrete value represented by the input masks. Fully-known arithmetic takes
 * the fast path used by normal execution and differential runs. */
static Nba97GameMatchClocksWord add_words(Nba97GameMatchClocksWord left,
    Nba97GameMatchClocksWord right) {
    Nba97GameMatchClocksWord result;
    unsigned carry_mask = 1u;
    unsigned byte;
    result.word = left.word + right.word;
    result.known_mask = 0;
    if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
        result.known_mask = 0x0fu;
        return result;
    }
    for (byte = 0; byte < 4; ++byte) {
        unsigned next_carry_mask = 0;
        unsigned first_output = 0;
        int first = 1;
        int invariant = 1;
        unsigned ls = (left.known_mask & (1u << byte)) ?
            ((left.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
        unsigned rs = (right.known_mask & (1u << byte)) ?
            ((right.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
        unsigned carry;
        for (carry = 0; carry <= 1; ++carry) {
            unsigned a;
            if (!(carry_mask & (1u << carry)))
                continue;
            for (a = ls; a <= le; ++a) {
                unsigned b;
                for (b = rs; b <= re; ++b) {
                    unsigned sum = a + b + carry;
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
        }
        if (invariant)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
        carry_mask = next_carry_mask;
    }
    return result;
}

static Nba97GameMatchClocksWord subtract_words(
    Nba97GameMatchClocksWord left, Nba97GameMatchClocksWord right) {
    Nba97GameMatchClocksWord result;
    unsigned borrow_mask = 1u;
    unsigned byte;
    result.word = left.word - right.word;
    result.known_mask = 0;
    if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
        result.known_mask = 0x0fu;
        return result;
    }
    for (byte = 0; byte < 4; ++byte) {
        unsigned next_borrow_mask = 0;
        unsigned first_output = 0;
        int first = 1;
        int invariant = 1;
        unsigned ls = (left.known_mask & (1u << byte)) ?
            ((left.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
        unsigned rs = (right.known_mask & (1u << byte)) ?
            ((right.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
        unsigned borrow;
        for (borrow = 0; borrow <= 1; ++borrow) {
            unsigned a;
            if (!(borrow_mask & (1u << borrow)))
                continue;
            for (a = ls; a <= le; ++a) {
                unsigned b;
                for (b = rs; b <= re; ++b) {
                    int difference = (int)a - (int)b - (int)borrow;
                    unsigned output = (unsigned)difference & 0xffu;
                    next_borrow_mask |= 1u << (difference < 0);
                    if (first) {
                        first_output = output;
                        first = 0;
                    } else if (output != first_output) {
                        invariant = 0;
                    }
                }
            }
        }
        if (invariant)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
        borrow_mask = next_borrow_mask;
    }
    return result;
}

static Nba97GameMatchClocksWord add_constant(Nba97GameMatchClocksWord source,
    uint32_t constant) {
    Nba97GameMatchClocksWord value;
    set_known(&value, constant);
    return add_words(source, value);
}

static uint32_t width_mask(unsigned width) {
    return width == 4 ? UINT32_MAX :
        (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}

static int spend(Nba97GameMatchClocksRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameMatchClocksRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameMatchClocksWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameMatchClocksAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word & width_mask(width);
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask =
            (uint8_t)(value->known_mask & knowledge_mask(width));
        event->kind = kind;
    }
}

static int locate(Nba97GameMatchClocksRun* run, uint32_t address,
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

static int read_value(Nba97GameMatchClocksRun* run, uint32_t address,
    uint8_t width, uint32_t pc, Nba97GameMatchClocksWord* value) {
    Nba97GameMatchClocksWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, width, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameMatchClocksRun* run, uint32_t address,
    uint8_t width, uint32_t pc, const Nba97GameMatchClocksWord* value) {
    Nba97GameMatchClocksWord stored = *value;
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    stored.word &= width_mask(width);
    stored.known_mask =
        (uint8_t)(stored.known_mask & knowledge_mask(width));
    TRY(locate(run, address, width, width, pc, &data, &known));
    if (!known && stored.known_mask != knowledge_mask(width))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(stored.word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int stack_address(Nba97GameMatchClocksRun* run, uint32_t offset,
    uint32_t pc, uint32_t* address) {
    Nba97GameMatchClocksWord value = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameMatchClocksWord load_lh(Nba97GameMatchClocksWord raw) {
    Nba97GameMatchClocksWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GameMatchClocksWord load_lbu(Nba97GameMatchClocksWord raw) {
    Nba97GameMatchClocksWord result;
    result.word = raw.word & 0xffu;
    result.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
    return result;
}

static int64_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GameMatchClocksWord* value,
    int64_t* minimum, int64_t* maximum) {
    uint32_t low = 0;
    uint32_t high = 0;
    unsigned i;
    if (!(value->known_mask & 8u)) {
        *minimum = INT32_MIN;
        *maximum = INT32_MAX;
        return;
    }
    for (i = 0; i < 4; ++i) {
        uint32_t byte = (value->word >> (i * 8u)) & 0xffu;
        low |= ((value->known_mask & (1u << i)) ? byte : 0u) << (i * 8u);
        high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) <<
            (i * 8u);
    }
    *minimum = signed_word(low);
    *maximum = signed_word(high);
}

static Nba97GameMatchClocksWord sltu_zero(
    const Nba97GameMatchClocksWord* value) {
    Nba97GameMatchClocksWord result;
    unsigned i;
    result.word = value->word != 0;
    result.known_mask = 0x0eu;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (i * 8u)) & 0xffu)) {
            set_known(&result, 1);
            return result;
        }
    if (value->known_mask == 0x0fu)
        set_known(&result, 0);
    return result;
}

static Nba97GameMatchClocksWord signed_less_constant(
    const Nba97GameMatchClocksWord* value, int32_t constant) {
    Nba97GameMatchClocksWord result;
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    result.word = signed_word(value->word) < constant;
    result.known_mask = 0x0eu;
    if (maximum < constant)
        set_known(&result, 1);
    else if (minimum >= constant)
        set_known(&result, 0);
    return result;
}

static Nba97GameMatchClocksWord signed_less_words(
    const Nba97GameMatchClocksWord* left,
    const Nba97GameMatchClocksWord* right) {
    Nba97GameMatchClocksWord result;
    int64_t left_min;
    int64_t left_max;
    int64_t right_min;
    int64_t right_max;
    signed_bounds(left, &left_min, &left_max);
    signed_bounds(right, &right_min, &right_max);
    result.word = signed_word(left->word) < signed_word(right->word);
    result.known_mask = 0x0eu;
    if (left_max < right_min)
        set_known(&result, 1);
    else if (left_min >= right_max)
        set_known(&result, 0);
    return result;
}

static int decide_zero(Nba97GameMatchClocksRun* run,
    const Nba97GameMatchClocksWord* value, uint32_t pc, int* is_zero) {
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
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int decide_equal(Nba97GameMatchClocksRun* run,
    const Nba97GameMatchClocksWord* left,
    const Nba97GameMatchClocksWord* right, uint32_t pc, int* equal) {
    unsigned i;
    for (i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((left->known_mask & right->known_mask & bit) &&
            ((left->word >> (i * 8u)) & 0xffu) !=
            ((right->word >> (i * 8u)) & 0xffu)) {
            *equal = 0;
            return NBA97_TEXT_COMPLETE;
        }
    }
    if (left->known_mask == 0x0fu && right->known_mask == 0x0fu) {
        *equal = left->word == right->word;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int decide_equal_constant(Nba97GameMatchClocksRun* run,
    const Nba97GameMatchClocksWord* value, uint32_t constant,
    uint32_t pc, int* equal) {
    Nba97GameMatchClocksWord known;
    set_known(&known, constant);
    return decide_equal(run, value, &known, pc, equal);
}

static int decide_nonpositive(Nba97GameMatchClocksRun* run,
    const Nba97GameMatchClocksWord* value, uint32_t pc, int* nonpositive) {
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    if (maximum <= 0) {
        *nonpositive = 1;
        return NBA97_TEXT_COMPLETE;
    }
    if (minimum > 0) {
        *nonpositive = 0;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static Nba97GameMatchClocksWord sra_word(Nba97GameMatchClocksWord value,
    unsigned shift) {
    Nba97GameMatchClocksWord result;
    unsigned byte;
    result.word = value.word >> shift;
    if (value.word & UINT32_C(0x80000000))
        result.word |= ~(UINT32_MAX >> shift);
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        unsigned low_bit = byte * 8u + shift;
        unsigned high_bit = low_bit + 7u;
        unsigned first_source = low_bit / 8u;
        unsigned last_source = high_bit < 32u ? high_bit / 8u : 3u;
        unsigned source;
        int known = 1;
        for (source = first_source; source <= last_source; ++source)
            if (!(value.known_mask & (1u << source)))
                known = 0;
        if (high_bit >= 32u && !(value.known_mask & 8u))
            known = 0;
        if (known)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    }
    return result;
}

static Nba97GameMatchClocksWord sll16(Nba97GameMatchClocksWord value) {
    Nba97GameMatchClocksWord result;
    result.word = value.word << 16u;
    result.known_mask = (uint8_t)(3u | ((value.known_mask & 3u) << 2u));
    return result;
}

static Nba97GameMatchClocksMultiplyTrace* begin_mult(
    Nba97GameMatchClocksRun* run, uint32_t pc,
    Nba97GameMatchClocksWord multiplicand,
    Nba97GameMatchClocksWord multiplier) {
    Nba97GameMatchClocksMultiplyTrace* trace =
        &run->out->multiply[run->out->multiply_count++];
    uint64_t product = (uint64_t)(signed_word(multiplicand.word) *
        signed_word(multiplier.word));
    memset(trace, 0, sizeof *trace);
    trace->pc = pc;
    trace->multiplicand = multiplicand;
    trace->multiplier = multiplier;
    run->machine.lo.word = (uint32_t)product;
    run->machine.hi.word = (uint32_t)(product >> 32u);
    run->machine.lo.known_mask = run->machine.hi.known_mask =
        (multiplicand.known_mask == 0x0fu &&
         multiplier.known_mask == 0x0fu) ? 0x0fu : 0;
    trace->hi = run->machine.hi;
    trace->lo = run->machine.lo;
    publish(run);
    return trace;
}

static void finish_divide(Nba97GameMatchClocksRun* run,
    Nba97GameMatchClocksMultiplyTrace* trace,
    unsigned destination, int sign_before_mfhi) {
    Nba97GameMatchClocksWord source = trace->multiplicand;
    Nba97GameMatchClocksWord sign;
    Nba97GameMatchClocksWord adjusted;
    Nba97GameMatchClocksWord shifted;
    if (sign_before_mfhi)
        R(NBA97_MATCH_INITIALIZE_V1) = sra_word(source, 31);
    R(NBA97_MATCH_INITIALIZE_T0) = run->machine.hi;
    trace->mfhi = R(NBA97_MATCH_INITIALIZE_T0);
    adjusted = add_words(R(NBA97_MATCH_INITIALIZE_T0), source);
    R(NBA97_MATCH_INITIALIZE_V0) = adjusted;
    shifted = sra_word(R(NBA97_MATCH_INITIALIZE_V0), 5);
    R(NBA97_MATCH_INITIALIZE_V0) = shifted;
    if (sign_before_mfhi)
        sign = R(NBA97_MATCH_INITIALIZE_V1);
    else {
        sign = sra_word(source, 31);
        R(NBA97_MATCH_INITIALIZE_V1) = sign;
    }
    R(destination) = subtract_words(R(NBA97_MATCH_INITIALIZE_V0), sign);
    trace->adjusted = adjusted;
    trace->shifted = shifted;
    trace->sign = sign;
    trace->seconds = R(destination);
    publish(run);
}

static int invoke(Nba97GameMatchClocksRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint32_t argument) {
    Nba97GameMatchClocksEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), argument); /* JAL delay slot. */
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[kind] + 1u;
    event.kind = kind;
    event.argument_count = 1;
    publish(run);
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user, &run->context->memory,
        &event, &run->machine);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!machine_valid(&run->machine))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[kind];
    return NBA97_TEXT_COMPLETE;
}

static int restore(Nba97GameMatchClocksRun* run, uint32_t pc,
    uint32_t offset, unsigned reg, Nba97GameMatchClocksWord* reported) {
    uint32_t address;
    TRY(stack_address(run, offset, pc, &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_clocks(Nba97GameMatchClocksContext* context,
    Nba97GameMatchClocksProgress* out) {
    Nba97GameMatchClocksRun storage;
    Nba97GameMatchClocksRun* run = &storage;
    Nba97GameMatchClocksWord value;
    Nba97GameMatchClocksWord branch_value;
    Nba97GameMatchClocksWord old_seconds;
    Nba97GameMatchClocksMultiplyTrace* trace;
    uint32_t address;
    int branch;
    int decision;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80067A60..0x80067A88: s2 is saved before a0 is captured;
     * the main clock and phase reads precede the remaining frame saves. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffd0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(stack_address(run, 0x28u, UINT32_C(0x80067a64), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067a64),
        &R(NBA97_GAME_MATCH_CLOCKS_S2)));
    R(NBA97_GAME_MATCH_CLOCKS_S2) = R(NBA97_MATCH_INITIALIZE_A0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
    TRY(read_value(run, MAIN_CLOCK, 4, UINT32_C(0x80067a70),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE, 2, UINT32_C(0x80067a78), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lh(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x82u);
    TRY(stack_address(run, 0x2cu, UINT32_C(0x80067a80), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067a80),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(stack_address(run, 0x24u, UINT32_C(0x80067a84), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067a84),
        &R(NBA97_GAME_MATCH_CLOCKS_S1)));
    TRY(stack_address(run, 0x20u, UINT32_C(0x80067a88), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067a88),
        &R(NBA97_MATCH_INITIALIZE_S0)));

    /* 0x80067A8C..0x80067B04: preserve the phase-82 special gates, the
     * phase<0x80 signed gate, and A2's unconditional exit-delay LUI. */
    R(NBA97_MATCH_INITIALIZE_A1) = sltu_zero(
        &R(NBA97_MATCH_INITIALIZE_A0)); /* 0x80067A90 delay */
    decision = decide_equal_constant(run, &R(NBA97_MATCH_INITIALIZE_V1),
        0x82u, UINT32_C(0x80067a8c), &branch);
    if (decision != NBA97_TEXT_COMPLETE)
        return decision;
    if (branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
        TRY(read_value(run, PHASE_82_BLOCK, 2, UINT32_C(0x80067a98),
            &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
        TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80067aa0), &branch));
        if (!branch)
            set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
        else {
            set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
            TRY(read_value(run, PHASE_82_ENABLE, 1,
                UINT32_C(0x80067aac), &value));
            R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
            TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
                UINT32_C(0x80067ab4), &branch));
            if (branch)
                set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
            else {
                set_known(&R(NBA97_MATCH_INITIALIZE_V0),
                    UINT32_C(0x80100000));
                TRY(read_value(run, PHASE_82_LIMIT, 4,
                    UINT32_C(0x80067ac0), &R(NBA97_MATCH_INITIALIZE_V0)));
                R(NBA97_MATCH_INITIALIZE_V0) = signed_less_words(
                    &R(NBA97_MATCH_INITIALIZE_V0),
                    &R(NBA97_MATCH_INITIALIZE_A0));
                TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
                    UINT32_C(0x80067acc), &branch));
                if (branch)
                    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
                else {
                    set_known(&R(NBA97_MATCH_INITIALIZE_V0),
                        UINT32_C(0x80100000));
                    TRY(read_value(run, PHASE_82_STOP, 4,
                        UINT32_C(0x80067ad8),
                        &R(NBA97_MATCH_INITIALIZE_V0)));
                    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_A0),
                        &R(NBA97_MATCH_INITIALIZE_V0),
                        UINT32_C(0x80067ae0), &branch));
                    if (branch)
                        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
                }
            }
        }
    } else {
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_V1), 0x80);
        TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80067af4), &branch));
        if (branch)
            set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x88880000));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_A1),
        UINT32_C(0x80067b00), &branch));
    if (branch)
        goto restore_registers;
    out->main_clock_eligible = 1;

    /* 0x80067B08..0x80067B48: retain the exact signed multiply division
     * sequence, signed minimum selection, and branch-delay clock store. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_A3) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0xffffdb58));
    TRY(read_value(run, MAIN_CLOCK, 4, UINT32_C(0x80067b10),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    R(NBA97_MATCH_INITIALIZE_A2) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x00008889));
    trace = begin_mult(run, UINT32_C(0x80067b18),
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_A2));
    finish_divide(run, trace, NBA97_GAME_MATCH_CLOCKS_S1, 1);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_words(
        &R(NBA97_MATCH_INITIALIZE_A0), &R(NBA97_GAME_MATCH_CLOCKS_S2));
    R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_MATCH_CLOCKS_S2);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067b34), &branch));
    if (!branch)
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_A0);
    R(NBA97_MATCH_INITIALIZE_V1) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_A1));
    TRY(write_value(run, MAIN_CLOCK, 4, UINT32_C(0x80067b48),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x80067b44), &branch));
    if (branch) {
        TRY(invoke(run, UINT32_C(0x80067bc8), UINT32_C(0x80029258),
            NBA97_GAME_MATCH_CLOCKS_CHILD_80029258, 10));
        goto shot_clock;
    }

    trace = begin_mult(run, UINT32_C(0x80067b4c),
        R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_A2));
    finish_divide(run, trace, NBA97_MATCH_INITIALIZE_S0, 0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_S0), 4);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067b68), &branch));
    if (branch) {
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_S0), 120); /* branch target 0x7B80 */
    } else {
        Nba97GameMatchClocksWord compare_left =
            R(NBA97_MATCH_INITIALIZE_S0);
        Nba97GameMatchClocksWord compare_right =
            R(NBA97_GAME_MATCH_CLOCKS_S1);
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_S0), 120); /* branch delay */
        decision = decide_equal(run, &compare_left, &compare_right,
            UINT32_C(0x80067b70), &branch);
        if (decision != NBA97_TEXT_COMPLETE)
            return decision;
        if (!branch) {
            TRY(invoke(run, UINT32_C(0x80067b78), UINT32_C(0x80029258),
                NBA97_GAME_MATCH_CLOCKS_CHILD_80029258, 11));
            R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
                &R(NBA97_MATCH_INITIALIZE_S0), 120);
        }
    }
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_GAME_MATCH_CLOCKS_S1), 120); /* 0x80067B88 delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067b84), &branch));
    if (!branch) {
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_S0), 60); /* 0x80067B90 delay */
        TRY(decide_zero(run, &branch_value, UINT32_C(0x80067b8c), &branch));
        if (branch) {
            TRY(invoke(run, UINT32_C(0x80067b94), UINT32_C(0x8007f9c4),
                NBA97_GAME_MATCH_CLOCKS_CHILD_8007F9C4, 2));
            goto shot_clock;
        }
    } else {
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_S0), 60);
    }
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_GAME_MATCH_CLOCKS_S1), 60); /* 0x80067BAC delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067ba8), &branch));
    if (!branch) {
        TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80067bb0), &branch));
        if (branch)
            TRY(invoke(run, UINT32_C(0x80067bb8), UINT32_C(0x8007f9c4),
                NBA97_GAME_MATCH_CLOCKS_CHILD_8007F9C4, 1));
    }

shot_clock:
    /* 0x80067BD0..0x80067CAC: shot-clock threshold logic repeats the exact
     * MULT sequence and deliberately sign-extends only the new low half. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_A3) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0xffffdba4));
    TRY(read_value(run, SHOT_CLOCK, 4, UINT32_C(0x80067bd8),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_A0),
        UINT32_C(0x80067be0), &branch));
    if (branch)
        goto team_clocks;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE, 2, UINT32_C(0x80067bec), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 0x80);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x88880000));
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067bf8), &branch));
    if (branch)
        goto team_clocks;
    out->shot_clock_eligible = 1;
    R(NBA97_MATCH_INITIALIZE_A2) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x00008889));
    trace = begin_mult(run, UINT32_C(0x80067c04),
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_A2));
    finish_divide(run, trace, NBA97_GAME_MATCH_CLOCKS_S1, 1);
    old_seconds = R(NBA97_GAME_MATCH_CLOCKS_S1);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_words(
        &R(NBA97_MATCH_INITIALIZE_A0), &R(NBA97_GAME_MATCH_CLOCKS_S2));
    R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_MATCH_CLOCKS_S2);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067c20), &branch));
    if (!branch)
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_A0);
    R(NBA97_MATCH_INITIALIZE_V1) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_A1));
    TRY(write_value(run, SHOT_CLOCK, 4, UINT32_C(0x80067c34),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x80067c30), &branch));
    if (branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
        TRY(read_value(run, SHOT_SOUND_ENABLE, 1,
            UINT32_C(0x80067c98), &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 10); /* branch delay */
        decision = decide_zero(run, &branch_value,
            UINT32_C(0x80067ca0), &branch);
        if (decision != NBA97_TEXT_COMPLETE)
            return decision;
        if (!branch)
            TRY(invoke(run, UINT32_C(0x80067ca8), UINT32_C(0x80029258),
                NBA97_GAME_MATCH_CLOCKS_CHILD_80029258, 10));
        goto team_clocks;
    }
    trace = begin_mult(run, UINT32_C(0x80067c38),
        R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_A2));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    TRY(read_value(run, SHOT_SOUND_ENABLE, 1,
        UINT32_C(0x80067c40), &value));
    R(NBA97_MATCH_INITIALIZE_A0) = load_lbu(value);
    finish_divide(run, trace, NBA97_MATCH_INITIALIZE_V1, 0);
    decision = decide_zero(run, &R(NBA97_MATCH_INITIALIZE_A0),
        UINT32_C(0x80067c54), &branch);
    /* The branch delay's quotient subtraction is already the finish result. */
    if (decision != NBA97_TEXT_COMPLETE)
        return decision;
    if (branch)
        goto team_clocks;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, MAIN_CLOCK, 4, UINT32_C(0x80067c60),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 301);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = sll16(
        R(NBA97_MATCH_INITIALIZE_V1)); /* branch delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067c6c), &branch));
    if (!branch)
        goto team_clocks;
    R(NBA97_MATCH_INITIALIZE_V1) = sra_word(
        R(NBA97_MATCH_INITIALIZE_V0), 16);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V1), 4);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067c7c), &branch));
    if (branch)
        goto team_clocks;
    branch_value = R(NBA97_MATCH_INITIALIZE_V1);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 11); /* branch delay */
    decision = decide_equal(run, &branch_value, &old_seconds,
        UINT32_C(0x80067c84), &branch);
    if (decision != NBA97_TEXT_COMPLETE)
        return decision;
    if (!branch)
        TRY(invoke(run, UINT32_C(0x80067ca8), UINT32_C(0x80029258),
            NBA97_GAME_MATCH_CLOCKS_CHILD_80029258, 11));

team_clocks:
    /* 0x80067CB0..0x80067D18: clear the shared signal first. Positive team
     * timers store the wrapped low half without clamping until a later call. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    R(NBA97_MATCH_INITIALIZE_A0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffeeb4));
    TRY(read_value(run, HOME_TIMER, 2, UINT32_C(0x80067cb8), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    set_known(&value, 0);
    TRY(write_value(run, TEAM_SIGNAL, 2, UINT32_C(0x80067cc0), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_nonpositive(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067cc4), &branch));
    if (!branch) {
        R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
            R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_GAME_MATCH_CLOCKS_S2));
        TRY(write_value(run, HOME_TIMER, 2, UINT32_C(0x80067cd4),
            &R(NBA97_MATCH_INITIALIZE_V0)));
    } else {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
        set_known(&value, 0);
        TRY(write_value(run, HOME_TIMER, 2, UINT32_C(0x80067cdc), &value));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
        TRY(write_value(run, HOME_STATE, 2, UINT32_C(0x80067ce4),
            &R(NBA97_MATCH_INITIALIZE_V0)));
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    R(NBA97_MATCH_INITIALIZE_A0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffef78));
    TRY(read_value(run, AWAY_TIMER, 2, UINT32_C(0x80067cf0), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_nonpositive(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067cf8), &branch));
    if (!branch) {
        R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
            R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_GAME_MATCH_CLOCKS_S2));
        TRY(write_value(run, AWAY_TIMER, 2, UINT32_C(0x80067d08),
            &R(NBA97_MATCH_INITIALIZE_V0)));
    } else {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
        set_known(&value, 0);
        TRY(write_value(run, AWAY_TIMER, 2, UINT32_C(0x80067d10), &value));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
        TRY(write_value(run, AWAY_STATE, 2, UINT32_C(0x80067d18),
            &R(NBA97_MATCH_INITIALIZE_V0)));
    }

restore_registers:
    /* 0x80067D1C..0x80067D34: every restore uses callback-mutable live sp;
     * ADDIU precedes the possibly unknown JR target and its NOP delay. */
    TRY(restore(run, UINT32_C(0x80067d1c), 0x2cu,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x80067d20), 0x28u,
        NBA97_GAME_MATCH_CLOCKS_S2, &out->restored_s2));
    TRY(restore(run, UINT32_C(0x80067d24), 0x24u,
        NBA97_GAME_MATCH_CLOCKS_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x80067d28), 0x20u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x30u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80067d30), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
