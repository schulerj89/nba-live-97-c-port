#include "game_clock_violations.h"

#include <limits.h>
#include <string.h>

#define MAIN_CLOCK UINT32_C(0x800fdb58)
#define PHASE UINT32_C(0x800fdb90)
#define TEAM UINT32_C(0x800fdb94)
#define SHOT_CLOCK UINT32_C(0x800fdba4)
#define PHASE_82_TIMER UINT32_C(0x800fdba8)
#define FINAL_TIMER UINT32_C(0x800fdbaa)
#define OWNER UINT32_C(0x800fdbcc)
#define BALL_POINTER UINT32_C(0x800fdc48)
#define ACTOR_POINTER UINT32_C(0x800fdc34)
#define SHOT_ENABLE UINT32_C(0x80021d92)
#define PHASE_82_ENABLE UINT32_C(0x80021d90)
#define FINAL_ENABLE UINT32_C(0x80021d91)
#define VIOLATION_STATE UINT32_C(0x800fe882)
#define PHASE_82_STATE UINT32_C(0x800fe884)
#define PHASE_82_BLOCK UINT32_C(0x800fe88e)
#define FINAL_BLOCK UINT32_C(0x800fe8e0)

typedef struct Run {
    Nba97GameClockViolationsContext* context;
    Nba97GameClockViolationsProgress* out;
    Nba97GameClockViolationsMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_status_ = (expression); \
    if (nba97_status_ != NBA97_TEXT_COMPLETE) return nba97_status_; \
} while (0)

static void publish(Run* run) { run->out->machine = run->machine; }

static void stop(Run* run, uint32_t pc, uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameClockViolationsWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameClockViolationsMachine* machine) {
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

static int validate(Nba97GameClockViolationsContext* context,
    Nba97GameClockViolationsProgress* out, Run* run) {
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

/* Exact byte-domain carry propagation preserves every known result byte. */
static Nba97GameClockViolationsWord add_words(
    Nba97GameClockViolationsWord left, Nba97GameClockViolationsWord right) {
    Nba97GameClockViolationsWord result;
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

static Nba97GameClockViolationsWord subtract_words(
    Nba97GameClockViolationsWord left, Nba97GameClockViolationsWord right) {
    Nba97GameClockViolationsWord result;
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

static Nba97GameClockViolationsWord add_constant(
    Nba97GameClockViolationsWord source, uint32_t constant) {
    Nba97GameClockViolationsWord value;
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

static void journal(Run* run, uint8_t kind, uint32_t pc,
    uint32_t address, uint8_t width,
    const Nba97GameClockViolationsWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameClockViolationsAccess* event =
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
    uint32_t pc, Nba97GameClockViolationsWord* value) {
    Nba97GameClockViolationsWord loaded = {0, 0};
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
    journal(run, NBA97_GAME_CLOCK_VIOLATIONS_READ, pc, address, width, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Run* run, uint32_t address, uint8_t width,
    uint32_t pc, const Nba97GameClockViolationsWord* value) {
    Nba97GameClockViolationsWord stored = *value;
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
    journal(run, NBA97_GAME_CLOCK_VIOLATIONS_STORE, pc, address, width,
        &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int address_from(Run* run, Nba97GameClockViolationsWord base,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GameClockViolationsWord value = add_constant(base, offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameClockViolationsWord load_lh(
    Nba97GameClockViolationsWord raw) {
    Nba97GameClockViolationsWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GameClockViolationsWord load_lhu(
    Nba97GameClockViolationsWord raw) {
    Nba97GameClockViolationsWord result;
    result.word = raw.word & 0xffffu;
    result.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
    return result;
}

static Nba97GameClockViolationsWord load_lbu(
    Nba97GameClockViolationsWord raw) {
    Nba97GameClockViolationsWord result;
    result.word = raw.word & 0xffu;
    result.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
    return result;
}

static int64_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GameClockViolationsWord* value,
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

static Nba97GameClockViolationsWord signed_less_constant(
    const Nba97GameClockViolationsWord* value, int32_t constant) {
    Nba97GameClockViolationsWord result;
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

static Nba97GameClockViolationsWord sra_word(
    Nba97GameClockViolationsWord value, unsigned shift) {
    Nba97GameClockViolationsWord result;
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

static Nba97GameClockViolationsWord sll16(
    Nba97GameClockViolationsWord value) {
    Nba97GameClockViolationsWord result;
    result.word = value.word << 16u;
    result.known_mask = (uint8_t)(3u | ((value.known_mask & 3u) << 2u));
    return result;
}

static int decide_zero(Run* run,
    const Nba97GameClockViolationsWord* value, uint32_t pc, int* is_zero) {
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

static int decide_equal(Run* run,
    const Nba97GameClockViolationsWord* left,
    const Nba97GameClockViolationsWord* right, uint32_t pc, int* equal) {
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

static int decide_nonnegative(Run* run,
    const Nba97GameClockViolationsWord* value, uint32_t pc,
    int* nonnegative) {
    Nba97GameClockViolationsWord negative = signed_less_constant(value, 0);
    int zero;
    TRY(decide_zero(run, &negative, pc, &zero));
    *nonnegative = zero;
    return NBA97_TEXT_COMPLETE;
}

static int decide_positive(Run* run,
    const Nba97GameClockViolationsWord* value, uint32_t pc, int* positive) {
    Nba97GameClockViolationsWord below_one = signed_less_constant(value, 1);
    int zero;
    TRY(decide_zero(run, &below_one, pc, &zero));
    *positive = zero;
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Run* run, uint32_t pc, uint32_t entry, uint8_t kind,
    uint8_t argument_count, int delay_sets_a0, uint32_t delay_a0) {
    Nba97GameClockViolationsEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    if (delay_sets_a0)
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), delay_a0);
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

static int effect_sequence(Run* run, int team_zero, uint32_t event_pc_zero,
    uint32_t event_pc_nonzero, uint32_t duration_pc, uint32_t start_pc,
    uint32_t start_argument, uint32_t finish_pc) {
    if (team_zero) {
        TRY(invoke(run, event_pc_zero, UINT32_C(0x80029590),
            NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80029590, 1, 1, 11));
        /* The J delay slot after the zero-team call replaces child a0. */
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 5000);
    } else {
        TRY(invoke(run, event_pc_nonzero, UINT32_C(0x80029590),
            NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80029590, 1, 1, 12));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 20000);
    }
    TRY(invoke(run, duration_pc, UINT32_C(0x800295c8),
        NBA97_GAME_CLOCK_VIOLATIONS_CHILD_800295C8, 1, 0, 0));
    TRY(invoke(run, start_pc, UINT32_C(0x80062300),
        NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80062300, 1, 1,
        start_argument));
    TRY(invoke(run, finish_pc, UINT32_C(0x80062660),
        NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80062660, 0, 0, 0));
    return NBA97_TEXT_COMPLETE;
}

static int restore(Run* run, uint32_t pc, uint32_t offset,
    unsigned reg, Nba97GameClockViolationsWord* reported) {
    uint32_t address;
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), offset, pc,
        &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_clock_violations(Nba97GameClockViolationsContext* context,
    Nba97GameClockViolationsProgress* out) {
    Run storage;
    Run* run = &storage;
    Nba97GameClockViolationsWord value;
    Nba97GameClockViolationsWord branch_value;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80067D38..0x80067D50: the main clock read occurs before
     * frame allocation, and the BEQ delay always spills ra. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, MAIN_CLOCK, 4, UINT32_C(0x80067d3c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
        UINT32_C(0x80067d44), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067d44),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A0);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u,
        UINT32_C(0x80067d50), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80067d50),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067d4c), &branch));
    if (branch)
        goto restore_registers;

    /* 0x80067D54..0x80067E14: shot-clock-zero violation. The negative-owner
     * path dereferences the live ball pointer without bounds checks. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, SHOT_CLOCK, 4, UINT32_C(0x80067d58),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067d60), &branch));
    if (!branch)
        goto phase_82;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_value(run, SHOT_ENABLE, 1, UINT32_C(0x80067d6c), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067d74), &branch));
    if (branch)
        goto phase_82;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, OWNER, 2, UINT32_C(0x80067d80), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_nonnegative(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067d88), &branch));
    if (!branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
        TRY(read_value(run, BALL_POINTER, 4, UINT32_C(0x80067d94),
            &R(NBA97_MATCH_INITIALIZE_V1)));
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V1), 0x10u,
            UINT32_C(0x80067d9c), &address));
        TRY(read_value(run, address, 4, UINT32_C(0x80067d9c),
            &R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_V0) = sra_word(
            R(NBA97_MATCH_INITIALIZE_V0), 8);
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_V0), 49);
        TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80067dac), &branch));
        if (branch)
            goto phase_82;
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V1), 0x18u,
            UINT32_C(0x80067db4), &address));
        TRY(read_value(run, address, 2, UINT32_C(0x80067db4), &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
        TRY(decide_positive(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80067dbc), &branch));
        if (branch)
            goto phase_82;
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, TEAM, 2, UINT32_C(0x80067dc8), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067dd0), &branch));
    TRY(effect_sequence(run, branch, UINT32_C(0x80067dd8),
        UINT32_C(0x80067de8), UINT32_C(0x80067df4),
        UINT32_C(0x80067dfc), 10, UINT32_C(0x80067e04)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 3);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, VIOLATION_STATE, 2, UINT32_C(0x80067e14),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->first_violation_triggered = 1;

phase_82:
    /* 0x80067E18..0x80067F30: the phase-82 timer writes its wrapped low
     * half before testing sign. The underflow branch clears phase through a0
     * in the team branch delay slot, before any callback can run. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_A0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffdb90));
    TRY(read_value(run, PHASE, 2, UINT32_C(0x80067e20), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lh(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x82u);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V1),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80067e28), &branch));
    if (!branch)
        goto final_timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, OWNER, 2, UINT32_C(0x80067e34), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_nonnegative(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067e3c), &branch));
    if (!branch)
        goto final_timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, ACTOR_POINTER, 4, UINT32_C(0x80067e48),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V0), 0xa0u,
        UINT32_C(0x80067e50), &address));
    TRY(read_value(run, address, 2, UINT32_C(0x80067e50), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2); /* 0x67E5C delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80067e58), &branch));
    if (!branch)
        goto final_timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE_82_STATE, 2, UINT32_C(0x80067e64), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lh(value);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V1),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80067e6c), &branch));
    if (!branch)
        goto final_timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE_82_BLOCK, 2, UINT32_C(0x80067e78), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067e80), &branch));
    if (!branch)
        goto final_timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE_82_TIMER, 2, UINT32_C(0x80067e8c), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
    R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_S0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, PHASE_82_TIMER, 2, UINT32_C(0x80067e9c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->phase_82_timer_decremented = 1;
    R(NBA97_MATCH_INITIALIZE_V0) = sll16(
        R(NBA97_MATCH_INITIALIZE_V0));
    TRY(decide_nonnegative(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067ea4), &branch));
    if (branch)
        goto final_timer;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_value(run, PHASE_82_ENABLE, 1, UINT32_C(0x80067eb0), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067eb8), &branch));
    if (!branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
        TRY(read_value(run, TEAM, 2, UINT32_C(0x80067ec4), &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&value, 0);
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0,
            UINT32_C(0x80067ed0), &address));
        TRY(write_value(run, address, 2, UINT32_C(0x80067ed0), &value));
        TRY(decide_zero(run, &branch_value, UINT32_C(0x80067ecc), &branch));
        TRY(effect_sequence(run, branch, UINT32_C(0x80067ed4),
            UINT32_C(0x80067ee4), UINT32_C(0x80067ef0),
            UINT32_C(0x80067ef8), 11, UINT32_C(0x80067f00)));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
        R(NBA97_MATCH_INITIALIZE_A0) = add_constant(
            R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffe882));
        TRY(read_value(run, VIOLATION_STATE, 2, UINT32_C(0x80067f10),
            &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_V0), 2);
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), 1); /* 0x67F20 delay */
        TRY(decide_zero(run, &branch_value, UINT32_C(0x80067f1c), &branch));
        if (branch)
            set_known(&R(NBA97_MATCH_INITIALIZE_V1), 3);
        TRY(write_value(run, VIOLATION_STATE, 2,
            UINT32_C(0x80067f28), &R(NBA97_MATCH_INITIALIZE_V1)));
        out->phase_82_violation_triggered = 1;
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    set_known(&value, 0);
    TRY(write_value(run, PHASE_82_TIMER, 2, UINT32_C(0x80067f30), &value));

final_timer:
    /* 0x80067F34..0x80068004: the final timer is independently gated and
     * also clears after underflow even when owner/enable suppress effects. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE, 2, UINT32_C(0x80067f38), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 0x80);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067f44), &branch));
    if (branch)
        goto restore_registers;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, FINAL_BLOCK, 2, UINT32_C(0x80067f50), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067f58), &branch));
    if (!branch)
        goto restore_registers;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, FINAL_TIMER, 2, UINT32_C(0x80067f64), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
    R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_S0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, FINAL_TIMER, 2, UINT32_C(0x80067f74),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->final_timer_decremented = 1;
    R(NBA97_MATCH_INITIALIZE_V0) = sll16(
        R(NBA97_MATCH_INITIALIZE_V0));
    TRY(decide_nonnegative(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067f7c), &branch));
    if (branch)
        goto restore_registers;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, OWNER, 2, UINT32_C(0x80067f88), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    TRY(decide_nonnegative(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80067f90), &branch));
    if (branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
        TRY(read_value(run, FINAL_ENABLE, 1, UINT32_C(0x80067f9c), &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
        TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80067fa4), &branch));
        if (!branch) {
            set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
            TRY(read_value(run, TEAM, 2, UINT32_C(0x80067fb0), &value));
            R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
            TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
                UINT32_C(0x80067fb8), &branch));
            TRY(effect_sequence(run, branch, UINT32_C(0x80067fc0),
                UINT32_C(0x80067fd0), UINT32_C(0x80067fdc),
                UINT32_C(0x80067fe4), 12, UINT32_C(0x80067fec)));
            set_known(&R(NBA97_MATCH_INITIALIZE_V0), 4);
            set_known(&R(NBA97_MATCH_INITIALIZE_AT),
                UINT32_C(0x80100000));
            TRY(write_value(run, VIOLATION_STATE, 2,
                UINT32_C(0x80067ffc), &R(NBA97_MATCH_INITIALIZE_V0)));
            out->final_violation_triggered = 1;
        }
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    set_known(&value, 0);
    TRY(write_value(run, FINAL_TIMER, 2, UINT32_C(0x80068004), &value));

restore_registers:
    /* 0x80068008..0x80068018: both restores use callback-mutable live sp;
     * ADDIU completes before the possibly unknown JR target is consumed. */
    TRY(restore(run, UINT32_C(0x80068008), 0x14u,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x8006800c), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80068014), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
