#include "game_contact_dispatch.h"

#include <limits.h>
#include <string.h>

#define CONTACT_TABLE UINT32_C(0x800fdcbc)
#define BALL_POINTER UINT32_C(0x800fdc48)

typedef struct Run {
    Nba97GameContactDispatchContext* context;
    Nba97GameContactDispatchProgress* out;
    Nba97GameContactDispatchMachine machine;
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

static void set_known(Nba97GameContactDispatchWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameContactDispatchMachine* machine) {
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

static int validate(Nba97GameContactDispatchContext* context,
    Nba97GameContactDispatchProgress* out, Run* run) {
    size_t i;
    size_t j;
    if (!out) return NBA97_TEXT_ARGUMENT;
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

static Nba97GameContactDispatchWord add_words(
    Nba97GameContactDispatchWord left, Nba97GameContactDispatchWord right) {
    Nba97GameContactDispatchWord result;
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

static Nba97GameContactDispatchWord add_constant(
    Nba97GameContactDispatchWord source, uint32_t constant) {
    Nba97GameContactDispatchWord value;
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
    uint8_t width, const Nba97GameContactDispatchWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameContactDispatchAccess* event =
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
                if ((*known)[j] > 1) return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}

static int read_value(Run* run, uint32_t address, uint8_t width,
    uint32_t pc, Nba97GameContactDispatchWord* value) {
    Nba97GameContactDispatchWord loaded = {0, 0};
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
    uint32_t pc, const Nba97GameContactDispatchWord* value) {
    Nba97GameContactDispatchWord stored = *value;
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

static int register_address(Run* run, Nba97GameContactDispatchWord base,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GameContactDispatchWord value = add_constant(base, offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameContactDispatchWord load_lh(
    Nba97GameContactDispatchWord raw) {
    Nba97GameContactDispatchWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GameContactDispatchWord sll16(
    Nba97GameContactDispatchWord value) {
    Nba97GameContactDispatchWord result;
    result.word = value.word << 16u;
    result.known_mask = (uint8_t)(3u | ((value.known_mask & 3u) << 2u));
    return result;
}

static Nba97GameContactDispatchWord sign_extend_low16(
    Nba97GameContactDispatchWord shifted) {
    Nba97GameContactDispatchWord result;
    uint32_t value = shifted.word >> 16u;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)((shifted.known_mask >> 2u) & 3u);
    if (shifted.known_mask & 8u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GameContactDispatchWord sll2(
    Nba97GameContactDispatchWord value) {
    Nba97GameContactDispatchWord result;
    result.word = value.word << 2u;
    result.known_mask = 0;
    if (value.known_mask & 1u) result.known_mask |= 1u;
    if ((value.known_mask & 3u) == 3u) result.known_mask |= 2u;
    if ((value.known_mask & 6u) == 6u) result.known_mask |= 4u;
    if ((value.known_mask & 12u) == 12u) result.known_mask |= 8u;
    return result;
}

static int64_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GameContactDispatchWord* value,
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

static Nba97GameContactDispatchWord signed_less_constant(
    const Nba97GameContactDispatchWord* value, int32_t constant) {
    Nba97GameContactDispatchWord result;
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    result.word = signed_word(value->word) < constant;
    result.known_mask = 0x0eu;
    if (maximum < constant) set_known(&result, 1);
    else if (minimum >= constant) set_known(&result, 0);
    return result;
}

static int decide_zero(Run* run, const Nba97GameContactDispatchWord* value,
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

static int decide_equal(Run* run, const Nba97GameContactDispatchWord* left,
    const Nba97GameContactDispatchWord* right, uint32_t pc, int* equal) {
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

static void mask_low_byte(Nba97GameContactDispatchWord* value) {
    value->word &= 0xffu;
    value->known_mask = (uint8_t)((value->known_mask & 1u) | 0x0eu);
}

static int invoke(Run* run, uint32_t pc, uint32_t entry, uint8_t kind,
    unsigned delay_destination, Nba97GameContactDispatchWord delay_source) {
    Nba97GameContactDispatchEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    R(delay_destination) = delay_source;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[kind] + 1u;
    event.kind = kind;
    event.argument_count = 2;
    publish(run);
    if (!run->context->io) return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user, &run->context->memory,
        &event, &run->machine);
    publish(run);
    if (accepted != 1) return NBA97_TEXT_IO_REFUSED;
    if (!machine_valid(&run->machine)) return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[kind];
    return NBA97_TEXT_COMPLETE;
}

static int restore(Run* run, uint32_t pc, uint32_t offset, unsigned reg,
    Nba97GameContactDispatchWord* reported) {
    uint32_t address;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), offset, pc,
        &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

static int prepare_index(Run* run, Nba97GameContactDispatchWord counter,
    uint32_t branch_pc, int* finished) {
    Nba97GameContactDispatchWord branch_value;
    R(NBA97_MATCH_INITIALIZE_V0) = sll16(counter);
    R(NBA97_MATCH_INITIALIZE_V1) = sign_extend_low16(
        R(NBA97_MATCH_INITIALIZE_V0));
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V1), 12);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = sll2(
        R(NBA97_MATCH_INITIALIZE_V1)); /* bound branch delay */
    TRY(decide_zero(run, &branch_value, branch_pc, finished));
    return NBA97_TEXT_COMPLETE;
}

static int read_table(Run* run, uint32_t pc, unsigned destination) {
    uint32_t address;
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_AT) = add_words(
        R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_AT),
        UINT32_C(0xffffdcbc), pc, &address));
    TRY(read_value(run, address, 4, pc, &R(destination)));
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_contact_dispatch(Nba97GameContactDispatchContext* context,
    Nba97GameContactDispatchProgress* out) {
    Run storage;
    Run* run = &storage;
    Nba97GameContactDispatchWord value;
    Nba97GameContactDispatchWord branch_value;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* 0x80060FBC..0x80060FD0: establish the frame and clear only live s2. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x18u,
        UINT32_C(0x80060fc0), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80060fc0),
        &R(NBA97_GAME_MATCH_CLOCKS_S2)));
    set_known(&R(NBA97_GAME_MATCH_CLOCKS_S2), 0);
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x1cu,
        UINT32_C(0x80060fc8), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80060fc8),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u,
        UINT32_C(0x80060fcc), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80060fcc),
        &R(NBA97_GAME_MATCH_CLOCKS_S1)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
        UINT32_C(0x80060fd0), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80060fd0),
        &R(NBA97_MATCH_INITIALIZE_S0)));

outer_increment:
    /* 0x80060FD4..0x8006100C: increment the full word, compare/index through
     * its signed low half, and publish s1=s2 in the ball-test delay slot. */
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S2), 1);
outer_accept_increment:
    R(NBA97_GAME_MATCH_CLOCKS_S2) = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(prepare_index(run, R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80060fe8), &branch));
    if (branch) goto restore_registers;
    TRY(read_table(run, UINT32_C(0x80060ff8), NBA97_MATCH_INITIALIZE_S0));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, BALL_POINTER, 4, UINT32_C(0x80061000),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    branch_value = R(NBA97_MATCH_INITIALIZE_S0);
    R(NBA97_GAME_MATCH_CLOCKS_S1) = R(NBA97_GAME_MATCH_CLOCKS_S2);
    TRY(decide_equal(run, &branch_value, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80061008), &branch));
    if (branch) goto outer_ball;

    /* 0x80061010..0x80061080: scan later entries. Non-ball pairs dispatch
     * 5FAA8; a zero-B4 ball pair dispatches 60E8C. Both returns are masked to
     * their low byte before the live s1 increment in the continuation delay. */
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S1), 1);
inner_nonball_accept:
    R(NBA97_GAME_MATCH_CLOCKS_S1) = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(prepare_index(run, R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80061024), &branch));
    if (branch) goto outer_increment;
    TRY(read_table(run, UINT32_C(0x80061034), NBA97_MATCH_INITIALIZE_A1));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, BALL_POINTER, 4, UINT32_C(0x8006103c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_A1),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80061044), &branch));
    if (!branch) {
        TRY(invoke(run, UINT32_C(0x8006104c), UINT32_C(0x8005faa8),
            NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8,
            NBA97_MATCH_INITIALIZE_A0, R(NBA97_MATCH_INITIALIZE_S0)));
        mask_low_byte(&R(NBA97_MATCH_INITIALIZE_V0)); /* 0x80061058 delay */
    } else {
        TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_A1), 0xb4u,
            UINT32_C(0x8006105c), &address));
        TRY(read_value(run, address, 2, UINT32_C(0x8006105c), &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
            R(NBA97_GAME_MATCH_CLOCKS_S1), 1); /* 0x80061068 delay */
        TRY(decide_zero(run, &branch_value, UINT32_C(0x80061064), &branch));
        if (!branch) goto inner_nonball_accept;
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_A1);
        TRY(invoke(run, UINT32_C(0x80061070), UINT32_C(0x80060e8c),
            NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C,
            NBA97_MATCH_INITIALIZE_A1, R(NBA97_MATCH_INITIALIZE_S0)));
        mask_low_byte(&R(NBA97_MATCH_INITIALIZE_V0));
    }
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S1), 1); /* 0x80061080 delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8006107c), &branch));
    if (!branch) goto inner_nonball_accept;
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S2), 1); /* 0x80061088 delay */
    goto outer_accept_increment;

outer_ball:
    /* 0x8006108C..0x800610DC: an active ball skips its whole outer row.
     * Otherwise call 60E8C for later references until its low byte is zero. */
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_S0), 0xb4u,
        UINT32_C(0x8006108c), &address));
    TRY(read_value(run, address, 2, UINT32_C(0x8006108c), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S2), 1); /* 0x80061098 delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80061094), &branch));
    if (!branch) goto outer_accept_increment;

    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S1), 1);
outer_ball_inner_accept:
    R(NBA97_GAME_MATCH_CLOCKS_S1) = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(prepare_index(run, R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x800610b0), &branch));
    if (branch) goto outer_increment;
    TRY(read_table(run, UINT32_C(0x800610c0), NBA97_MATCH_INITIALIZE_A1));
    TRY(invoke(run, UINT32_C(0x800610c4), UINT32_C(0x80060e8c),
        NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C,
        NBA97_MATCH_INITIALIZE_A0, R(NBA97_MATCH_INITIALIZE_S0)));
    mask_low_byte(&R(NBA97_MATCH_INITIALIZE_V0));
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S1), 1); /* 0x800610D4 delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x800610d0), &branch));
    if (!branch) goto outer_ball_inner_accept;
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_GAME_MATCH_CLOCKS_S2), 1); /* 0x800610DC delay */
    goto outer_accept_increment;

restore_registers:
    /* 0x800610E0..0x800610F8: restore through callback-mutable sp and advance
     * it before consuming the possibly unknown JR target. */
    TRY(restore(run, UINT32_C(0x800610e0), 0x1cu,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x800610e4), 0x18u,
        NBA97_GAME_MATCH_CLOCKS_S2, &out->restored_s2));
    TRY(restore(run, UINT32_C(0x800610e8), 0x14u,
        NBA97_GAME_MATCH_CLOCKS_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x800610ec), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x800610f4), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
