#include "game_period_expiry.h"

#include <limits.h>
#include <string.h>

#define MAIN_CLOCK UINT32_C(0x800fdb58)
#define OWNER UINT32_C(0x800fdbcc)
#define ACTOR UINT32_C(0x800fdc34)
#define BALL UINT32_C(0x800fdc48)
#define PHASE UINT32_C(0x800fdb90)
#define PERIOD_GATE UINT32_C(0x800fa034)
#define PERIOD_FLAG UINT32_C(0x800fa038)
#define VIOLATION UINT32_C(0x800fe882)
#define PERIOD_ENABLE UINT32_C(0x80021d95)
#define PERIOD_TIMER UINT32_C(0x800fdb76)
#define DELTA UINT32_C(0x800fdb6c)

typedef struct Run {
    Nba97GamePeriodExpiryContext* context;
    Nba97GamePeriodExpiryProgress* out;
    Nba97GamePeriodExpiryMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Run* run) { run->out->machine = run->machine; }

static void stop(Run* run, uint32_t pc, uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GamePeriodExpiryWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GamePeriodExpiryMachine* machine) {
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

static int validate(Nba97GamePeriodExpiryContext* context,
    Nba97GamePeriodExpiryProgress* out, Run* run) {
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

static Nba97GamePeriodExpiryWord add_words(Nba97GamePeriodExpiryWord left,
    Nba97GamePeriodExpiryWord right) {
    Nba97GamePeriodExpiryWord result;
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
            if (!(carry_mask & (1u << carry))) continue;
            for (a = ls; a <= le; ++a) {
                unsigned b;
                for (b = rs; b <= re; ++b) {
                    unsigned sum = a + b + carry;
                    unsigned output = sum & 0xffu;
                    next_carry_mask |= 1u << (sum >> 8u);
                    if (first) { first_output = output; first = 0; }
                    else if (output != first_output) invariant = 0;
                }
            }
        }
        if (invariant)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
        carry_mask = next_carry_mask;
    }
    return result;
}

static Nba97GamePeriodExpiryWord subtract_words(
    Nba97GamePeriodExpiryWord left, Nba97GamePeriodExpiryWord right) {
    Nba97GamePeriodExpiryWord result;
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
            if (!(borrow_mask & (1u << borrow))) continue;
            for (a = ls; a <= le; ++a) {
                unsigned b;
                for (b = rs; b <= re; ++b) {
                    int difference = (int)a - (int)b - (int)borrow;
                    unsigned output = (unsigned)difference & 0xffu;
                    next_borrow_mask |= 1u << (difference < 0);
                    if (first) { first_output = output; first = 0; }
                    else if (output != first_output) invariant = 0;
                }
            }
        }
        if (invariant)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
        borrow_mask = next_borrow_mask;
    }
    return result;
}

static Nba97GamePeriodExpiryWord add_constant(
    Nba97GamePeriodExpiryWord source, uint32_t constant) {
    Nba97GamePeriodExpiryWord value;
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

static int spend(Run* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Run* run, uint8_t kind, uint32_t pc, uint32_t address,
    uint8_t width, const Nba97GamePeriodExpiryWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GamePeriodExpiryAccess* event =
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

static int locate(Run* run, uint32_t address, size_t width,
    size_t alignment, uint32_t pc, uint8_t** data, uint8_t** known) {
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

static int read_value(Run* run, uint32_t address, uint8_t width,
    uint32_t pc, Nba97GamePeriodExpiryWord* value) {
    Nba97GamePeriodExpiryWord loaded = {0, 0};
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

static int write_value(Run* run, uint32_t address, uint8_t width,
    uint32_t pc, const Nba97GamePeriodExpiryWord* value) {
    Nba97GamePeriodExpiryWord stored = *value;
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
        if (known) known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int register_address(Run* run, Nba97GamePeriodExpiryWord base,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GamePeriodExpiryWord value = add_constant(base, offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GamePeriodExpiryWord load_lh(Nba97GamePeriodExpiryWord raw) {
    Nba97GamePeriodExpiryWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GamePeriodExpiryWord load_lhu(Nba97GamePeriodExpiryWord raw) {
    Nba97GamePeriodExpiryWord result;
    result.word = raw.word & 0xffffu;
    result.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
    return result;
}

static Nba97GamePeriodExpiryWord load_lbu(Nba97GamePeriodExpiryWord raw) {
    Nba97GamePeriodExpiryWord result;
    result.word = raw.word & 0xffu;
    result.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
    return result;
}

static int64_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GamePeriodExpiryWord* value,
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
        high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) << (i * 8u);
    }
    *minimum = signed_word(low);
    *maximum = signed_word(high);
}

static int decide_zero(Run* run, const Nba97GamePeriodExpiryWord* value,
    uint32_t pc, int* is_zero) {
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

static int decide_equal(Run* run, const Nba97GamePeriodExpiryWord* left,
    const Nba97GamePeriodExpiryWord* right, uint32_t pc, int* equal) {
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

static int decide_equal_constant(Run* run,
    const Nba97GamePeriodExpiryWord* value, uint32_t constant,
    uint32_t pc, int* equal) {
    Nba97GamePeriodExpiryWord known;
    set_known(&known, constant);
    return decide_equal(run, value, &known, pc, equal);
}

static Nba97GamePeriodExpiryWord unsigned_less_constant(
    const Nba97GamePeriodExpiryWord* value, uint32_t constant) {
    Nba97GamePeriodExpiryWord result;
    uint32_t minimum = 0;
    uint32_t maximum = 0;
    unsigned i;
    for (i = 0; i < 4; ++i) {
        uint32_t byte = (value->word >> (i * 8u)) & 0xffu;
        minimum |= ((value->known_mask & (1u << i)) ? byte : 0u) << (i * 8u);
        maximum |= ((value->known_mask & (1u << i)) ? byte : 0xffu) << (i * 8u);
    }
    result.word = value->word < constant;
    result.known_mask = 0x0eu;
    if (maximum < constant) set_known(&result, 1);
    else if (minimum >= constant) set_known(&result, 0);
    return result;
}

static Nba97GamePeriodExpiryWord signed_less_constant(
    const Nba97GamePeriodExpiryWord* value, int32_t constant) {
    Nba97GamePeriodExpiryWord result;
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    result.word = signed_word(value->word) < constant;
    result.known_mask = 0x0eu;
    if (maximum < constant) set_known(&result, 1);
    else if (minimum >= constant) set_known(&result, 0);
    return result;
}

static Nba97GamePeriodExpiryWord sra_word(
    Nba97GamePeriodExpiryWord value, unsigned shift) {
    Nba97GamePeriodExpiryWord result;
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
            if (!(value.known_mask & (1u << source))) known = 0;
        if (high_bit >= 32u && !(value.known_mask & 8u)) known = 0;
        if (known)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    }
    return result;
}

static Nba97GamePeriodExpiryWord sll16(Nba97GamePeriodExpiryWord value) {
    Nba97GamePeriodExpiryWord result;
    result.word = value.word << 16u;
    result.known_mask = (uint8_t)(3u | ((value.known_mask & 3u) << 2u));
    return result;
}

static int invoke(Run* run) {
    Nba97GamePeriodExpiryEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800676d4));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 1); /* 0x800676D0 delay. */
    stop(run, UINT32_C(0x800676cc), 0, UINT32_C(0x800582dc));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = UINT32_C(0x800676cc);
    event.delay_slot_pc = UINT32_C(0x800676d0);
    event.entry = UINT32_C(0x800582dc);
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[
        NBA97_GAME_PERIOD_EXPIRY_CHILD_800582DC] + 1u;
    event.kind = NBA97_GAME_PERIOD_EXPIRY_CHILD_800582DC;
    event.argument_count = 2;
    publish(run);
    if (!run->context->io) return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user, &run->context->memory,
        &event, &run->machine);
    publish(run);
    if (accepted != 1) return NBA97_TEXT_IO_REFUSED;
    if (!machine_valid(&run->machine)) return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[NBA97_GAME_PERIOD_EXPIRY_CHILD_800582DC];
    return NBA97_TEXT_COMPLETE;
}

static int restore(Run* run, uint32_t pc, uint32_t offset, unsigned reg,
    Nba97GamePeriodExpiryWord* reported) {
    uint32_t address;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), offset, pc,
        &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_period_expiry(Nba97GamePeriodExpiryContext* context,
    Nba97GamePeriodExpiryProgress* out) {
    Run storage;
    Run* run = &storage;
    Nba97GamePeriodExpiryWord value;
    Nba97GamePeriodExpiryWord branch_value;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* 0x80067664..0x80067680: the clock read precedes frame allocation, and
     * the s0 spill is the main-clock BNE delay slot on both outcomes. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, MAIN_CLOCK, 4, UINT32_C(0x80067668),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u,
        UINT32_C(0x80067670), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067670),
        &R(NBA97_GAME_MATCH_CLOCKS_S1)));
    set_known(&R(NBA97_GAME_MATCH_CLOCKS_S1), 0);
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x18u,
        UINT32_C(0x80067678), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067678),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
        UINT32_C(0x80067680), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067680),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8006767c), &branch));
    if (!branch) goto return_value;

    /* 0x80067684..0x80067700: optional actor service and transition stores.
     * Type 19 still publishes a0 in its BEQ delay, while the post-child store
     * deliberately uses the live, callback-mutable s0. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, OWNER, 2, UINT32_C(0x80067688), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    branch_value = signed_less_constant(&R(NBA97_MATCH_INITIALIZE_V0), 0);
    TRY(decide_zero(run, &branch_value,
        UINT32_C(0x80067690), &branch));
    if (!branch) goto ball_gate;

    set_known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80100000));
    TRY(read_value(run, ACTOR, 4, UINT32_C(0x8006769c),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_S0), 0x1au,
        UINT32_C(0x800676a4), &address));
    TRY(read_value(run, address, 1, UINT32_C(0x800676a4), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lbu(value);
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffff2));
    R(NBA97_MATCH_INITIALIZE_V0) = unsigned_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 2);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 30); /* 0x800676B8 delay. */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x800676b4), &branch));
    if (branch) {
        R(NBA97_MATCH_INITIALIZE_V1).word &= 0xffu;
        R(NBA97_MATCH_INITIALIZE_V1).known_mask = (uint8_t)(
            (R(NBA97_MATCH_INITIALIZE_V1).known_mask & 1u) | 0x0eu);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 19);
        branch_value = R(NBA97_MATCH_INITIALIZE_V1);
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
        TRY(decide_equal_constant(run, &branch_value, 19,
            UINT32_C(0x800676c4), &branch));
        if (!branch) TRY(invoke(run));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 30);
    }
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_S0), 0xb4u,
        UINT32_C(0x800676d8), &address));
    TRY(write_value(run, address, 2, UINT32_C(0x800676d8),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, BALL, 4, UINT32_C(0x800676e0),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, OWNER, 2, UINT32_C(0x800676ec),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    set_known(&value, 0);
    TRY(write_value(run, PHASE, 2, UINT32_C(0x800676f4), &value));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, ACTOR, 4, UINT32_C(0x800676fc),
        &R(NBA97_MATCH_INITIALIZE_V1)));

ball_gate:
    /* 0x80067700..0x80067730: reload the ball pointer, then require signed
     * (height >> 8) < 49 and signed velocity >= 0. Both exits publish v0=s1
     * in their branch delay slots before an unknown decision can stop. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, BALL, 4, UINT32_C(0x80067704),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_V1), 0x10u,
        UINT32_C(0x8006770c), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x8006770c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = sra_word(
        R(NBA97_MATCH_INITIALIZE_V0), 8);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 49);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_GAME_MATCH_CLOCKS_S1);
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8006771c), &branch));
    if (branch) goto restore_registers;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_V1), 0x18u,
        UINT32_C(0x80067724), &address));
    TRY(read_value(run, address, 2, UINT32_C(0x80067724), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    branch_value = signed_less_constant(&R(NBA97_MATCH_INITIALIZE_V0), 0);
    R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_GAME_MATCH_CLOCKS_S1);
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8006772c), &branch));
    if (!branch) goto restore_registers;

    /* 0x80067734..0x80067788: publish the period flag only when every live
     * signed/unsigned gate passes; preserve the first BLTZ delay's v0=0x82. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, PERIOD_GATE, 4, UINT32_C(0x80067738),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    branch_value = signed_less_constant(&R(NBA97_MATCH_INITIALIZE_V0), 0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x82); /* 0x80067744 delay. */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067740), &branch));
    if (!branch) goto timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE, 2, UINT32_C(0x8006774c), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lh(value);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V1),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80067754), &branch));
    if (!branch) goto timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, VIOLATION, 2, UINT32_C(0x80067760), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067768), &branch));
    if (!branch) goto timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_value(run, PERIOD_ENABLE, 1, UINT32_C(0x80067774), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1); /* 0x80067780 delay. */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8006777c), &branch));
    if (!branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
        TRY(write_value(run, PERIOD_FLAG, 2, UINT32_C(0x80067788),
            &R(NBA97_MATCH_INITIALIZE_V0)));
    }

timer:
    /* 0x8006778C..0x800677BC: subtract unsigned halfwords with 32-bit wrap,
     * store the low half first, then classify that low half through SLL16. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffdb76));
    TRY(read_value(run, PERIOD_TIMER, 2, UINT32_C(0x80067794), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lhu(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
    TRY(read_value(run, DELTA, 2, UINT32_C(0x8006779c), &value));
    R(NBA97_MATCH_INITIALIZE_A0) = load_lhu(value);
    R(NBA97_MATCH_INITIALIZE_V1) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_A0));
    TRY(write_value(run, PERIOD_TIMER, 2, UINT32_C(0x800677a8),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V1) = sll16(
        R(NBA97_MATCH_INITIALIZE_V1));
    branch_value = signed_less_constant(&R(NBA97_MATCH_INITIALIZE_V1), 1);
    R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_GAME_MATCH_CLOCKS_S1);
    TRY(decide_zero(run, &branch_value, UINT32_C(0x800677b0), &branch));
    if (branch) goto restore_registers;
    set_known(&R(NBA97_GAME_MATCH_CLOCKS_S1), 1);

return_value:
    R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_GAME_MATCH_CLOCKS_S1);

restore_registers:
    /* 0x800677C0..0x800677D4: restore through live sp, advance it before the
     * possibly unknown JR target is consumed, and retain the source NOP. */
    TRY(restore(run, UINT32_C(0x800677c0), 0x18u,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x800677c4), 0x14u,
        NBA97_GAME_MATCH_CLOCKS_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x800677c8), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x800677d0), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
