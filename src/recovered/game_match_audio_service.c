#include "game_match_audio_service.h"

#include <limits.h>
#include <string.h>

#define CLOCK UINT32_C(0x800e430c)
#define PHASE UINT32_C(0x800170bc)
#define STREAM_HANDLE UINT32_C(0x8002149c)
#define AUDIO_RESULT UINT32_C(0x80021ee0)
#define CUE_VALUE UINT32_C(0x80021ee8)
#define AUDIO_MODE UINT32_C(0x800fda0c)
#define AUDIO_TIMER UINT32_C(0x800fda0e)
#define AUDIO_TIMER_RESET UINT32_C(0x800fda10)

typedef struct Run {
    Nba97GameMatchAudioServiceContext* context;
    Nba97GameMatchAudioServiceProgress* out;
    Nba97GameMatchAudioServiceMachine machine;
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

static void set_known(Nba97GameMatchAudioServiceWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameMatchAudioServiceMachine* machine) {
    unsigned i;
    if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask !=
            0x0fu ||
        machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine->registers.gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameMatchAudioServiceContext* context,
    Nba97GameMatchAudioServiceProgress* out, Run* run) {
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

/* Byte-domain carry and borrow enumeration keeps every invariant result byte
 * known for partially-known inputs without choosing values for unknown data. */
static Nba97GameMatchAudioServiceWord add_words(
    Nba97GameMatchAudioServiceWord left,
    Nba97GameMatchAudioServiceWord right) {
    Nba97GameMatchAudioServiceWord result;
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
            result.known_mask =
                (uint8_t)(result.known_mask | (1u << byte));
        carry_mask = next_carry_mask;
    }
    return result;
}

static Nba97GameMatchAudioServiceWord subtract_words(
    Nba97GameMatchAudioServiceWord left,
    Nba97GameMatchAudioServiceWord right) {
    Nba97GameMatchAudioServiceWord result;
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
            result.known_mask =
                (uint8_t)(result.known_mask | (1u << byte));
        borrow_mask = next_borrow_mask;
    }
    return result;
}

static Nba97GameMatchAudioServiceWord add_constant(
    Nba97GameMatchAudioServiceWord source, uint32_t constant) {
    Nba97GameMatchAudioServiceWord value;
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
    const Nba97GameMatchAudioServiceWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameMatchAudioServiceAccess* event =
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
    uint32_t pc, Nba97GameMatchAudioServiceWord* value) {
    Nba97GameMatchAudioServiceWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, width, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_MATCH_AUDIO_SERVICE_READ, pc, address, width,
        value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Run* run, uint32_t address, uint8_t width,
    uint32_t pc, const Nba97GameMatchAudioServiceWord* value) {
    Nba97GameMatchAudioServiceWord stored = *value;
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
    journal(run, NBA97_GAME_MATCH_AUDIO_SERVICE_STORE, pc, address, width,
        &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int address_from(Run* run, Nba97GameMatchAudioServiceWord base,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GameMatchAudioServiceWord value = add_constant(base, offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameMatchAudioServiceWord load_lh(
    Nba97GameMatchAudioServiceWord raw) {
    Nba97GameMatchAudioServiceWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ?
        value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GameMatchAudioServiceWord load_lhu(
    Nba97GameMatchAudioServiceWord raw) {
    Nba97GameMatchAudioServiceWord result;
    result.word = raw.word & 0xffffu;
    result.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
    return result;
}

static int64_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GameMatchAudioServiceWord* value,
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
        low |= ((value->known_mask & (1u << i)) ? byte : 0u) <<
            (i * 8u);
        high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) <<
            (i * 8u);
    }
    *minimum = signed_word(low);
    *maximum = signed_word(high);
}

static Nba97GameMatchAudioServiceWord signed_less_constant(
    const Nba97GameMatchAudioServiceWord* value, int32_t constant) {
    Nba97GameMatchAudioServiceWord result;
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

static Nba97GameMatchAudioServiceWord sll16(
    Nba97GameMatchAudioServiceWord value) {
    Nba97GameMatchAudioServiceWord result;
    result.word = value.word << 16u;
    result.known_mask = (uint8_t)(3u | ((value.known_mask & 3u) << 2u));
    return result;
}

static int decide_zero(Run* run,
    const Nba97GameMatchAudioServiceWord* value, uint32_t pc,
    int* is_zero) {
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
    const Nba97GameMatchAudioServiceWord* left,
    const Nba97GameMatchAudioServiceWord* right, uint32_t pc, int* equal) {
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
    const Nba97GameMatchAudioServiceWord* value, uint32_t pc,
    int* nonnegative) {
    Nba97GameMatchAudioServiceWord negative =
        signed_less_constant(value, 0);
    int zero;
    TRY(decide_zero(run, &negative, pc, &zero));
    *nonnegative = zero;
    return NBA97_TEXT_COMPLETE;
}

static int decide_positive(Run* run,
    const Nba97GameMatchAudioServiceWord* value, uint32_t pc,
    int* positive) {
    Nba97GameMatchAudioServiceWord below_one =
        signed_less_constant(value, 1);
    int zero;
    TRY(decide_zero(run, &below_one, pc, &zero));
    *positive = zero;
    return NBA97_TEXT_COMPLETE;
}

static void prepare_jal(Run* run, uint32_t pc) {
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    publish(run);
}

static int invoke_prepared(Run* run, uint32_t pc, uint32_t entry,
    uint8_t kind, uint8_t argument_count) {
    Nba97GameMatchAudioServiceEvent event;
    int accepted;
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

static int call_nop(Run* run, uint32_t pc, uint32_t entry, uint8_t kind,
    uint8_t argument_count) {
    prepare_jal(run, pc);
    return invoke_prepared(run, pc, entry, kind, argument_count);
}

static int restore(Run* run, uint32_t pc, uint32_t offset, unsigned reg,
    Nba97GameMatchAudioServiceWord* reported) {
    uint32_t address;
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), offset, pc,
        &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_audio_service(
    Nba97GameMatchAudioServiceContext* context,
    Nba97GameMatchAudioServiceProgress* out) {
    Run storage;
    Run* run = &storage;
    Nba97GameMatchAudioServiceWord value;
    Nba97GameMatchAudioServiceWord branch_value;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8002A264..0x8002A274: allocate the frame, save ra/s1,
     * assign JAL ra, and spill s0 in the clock call's delay slot. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x18u,
        UINT32_C(0x8002a268), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8002a268),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u,
        UINT32_C(0x8002a26c), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8002a26c),
        &R(NBA97_GAME_MATCH_CLOCKS_S1)));
    prepare_jal(run, UINT32_C(0x8002a270));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
        UINT32_C(0x8002a274), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8002a274),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    TRY(invoke_prepared(run, UINT32_C(0x8002a270),
        UINT32_C(0x800a5810),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800A5810, 0));

    /* 0x8002A278..0x8002A294: read the old clock after the child, load the
     * live signed mode through s0, publish raw child v0, and derive s1 with
     * wrapping SUBU. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800e0000));
    TRY(read_value(run, CLOCK, 4, UINT32_C(0x8002a27c),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_S0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xffffda0c));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_S0), 0,
        UINT32_C(0x8002a288), &address));
    TRY(read_value(run, address, 2, UINT32_C(0x8002a288), &value));
    R(NBA97_MATCH_INITIALIZE_A0) = load_lh(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
    TRY(write_value(run, CLOCK, 4, UINT32_C(0x8002a290),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_GAME_MATCH_CLOCKS_S1) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    out->clock_delta = R(NBA97_GAME_MATCH_CLOCKS_S1);

    /* 0x8002A298..0x8002A2B8: preserve all three branch delay results while
     * selecting signed mode 2, mode 1, mode 3+, or the quiet epilogue. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_A0), 3);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_A0), &branch_value,
        UINT32_C(0x8002a29c), &branch));
    if (branch)
        goto mode_2;
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8002a2a4), &branch));
    if (!branch) {
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x82);
        TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_A0), &branch_value,
            UINT32_C(0x8002a2ac), &branch));
        if (branch)
            goto mode_1;
        goto restore_registers;
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 3);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_A0),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x8002a2c0), &branch));
    if (!branch)
        goto restore_registers;

    /* 0x8002A2C8..0x8002A31C: mode 3's nested eligibility and readiness
     * gates. Each child result is consumed with the original signedness. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_value(run, STREAM_HANDLE, 4, UINT32_C(0x8002a2cc),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x8002a2d4), &branch));
    if (branch)
        goto mode_3_decrement;
    TRY(call_nop(run, UINT32_C(0x8002a2dc), UINT32_C(0x8008472c),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C, 0));
    TRY(decide_nonnegative(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x8002a2e4), &branch));
    if (!branch)
        goto mode_3_decrement;
    TRY(call_nop(run, UINT32_C(0x8002a2ec), UINT32_C(0x80088d0c),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C, 0));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x8002a2f4), &branch));
    if (branch)
        goto mode_3_pump;
    TRY(call_nop(run, UINT32_C(0x8002a2fc), UINT32_C(0x8008847c),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008847C, 0));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x8002a304), &branch));
    if (branch)
        goto mode_3_pump;
    TRY(call_nop(run, UINT32_C(0x8002a30c), UINT32_C(0x80084588),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80084588, 0));
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 2);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8002a318), &branch));
    if (!branch)
        goto mode_3_decrement;

mode_3_pump:
    /* 0x8002A320..0x8002A33C: pump, then copy the live unsigned reset
     * halfword to the active timer before joining the signed timer test. */
    TRY(call_nop(run, UINT32_C(0x8002a320), UINT32_C(0x80083eec),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC, 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, AUDIO_TIMER_RESET, 2, UINT32_C(0x8002a32c),
        &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, AUDIO_TIMER, 2, UINT32_C(0x8002a334),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    goto mode_3_timer_test;

mode_3_decrement:
    /* 0x8002A340..0x8002A354: reload the timer as unsigned, subtract the
     * callback-live s1 with SUBU, and store the wrapped low half. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_V1) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffda0e));
    TRY(read_value(run, AUDIO_TIMER, 2, UINT32_C(0x8002a348), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
    R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_GAME_MATCH_CLOCKS_S1));
    TRY(write_value(run, R(NBA97_MATCH_INITIALIZE_V1).word, 2,
        UINT32_C(0x8002a354), &R(NBA97_MATCH_INITIALIZE_V0)));

mode_3_timer_test:
    /* 0x8002A358..0x8002A3A4: the BGEZ delay publishes v0=0x82 even on
     * exit or an unknown sign. Negative timers cue only during phase 0x82;
     * every other phase clears the mode. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, AUDIO_TIMER, 2, UINT32_C(0x8002a35c), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x82);
    TRY(decide_nonnegative(run, &branch_value, UINT32_C(0x8002a364),
        &branch));
    if (branch)
        goto restore_registers;
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80010000));
    TRY(read_value(run, PHASE, 4, UINT32_C(0x8002a370),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V1),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x8002a378), &branch));
    if (branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020000));
        TRY(read_value(run, CUE_VALUE, 2, UINT32_C(0x8002a384), &value));
        R(NBA97_MATCH_INITIALIZE_A1) = load_lh(value);
        prepare_jal(run, UINT32_C(0x8002a388));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 1);
        TRY(invoke_prepared(run, UINT32_C(0x8002a388),
            UINT32_C(0x8002a46c),
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A46C, 2));
        goto restore_registers;
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    set_known(&value, 0);
    TRY(write_value(run, AUDIO_MODE, 2, UINT32_C(0x8002a39c), &value));
    goto restore_registers;

mode_1:
    /* 0x8002A3A8..0x8002A3D8: phase 0x82 skips the clamp. Other phases
     * clamp signed timers >=480 to 120; the BNE delay leaves v0=120. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80010000));
    TRY(read_value(run, PHASE, 4, UINT32_C(0x8002a3ac),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V1),
        &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x8002a3b4), &branch));
    if (!branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
        TRY(read_value(run, AUDIO_TIMER, 2, UINT32_C(0x8002a3c0), &value));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
        branch_value = signed_less_constant(
            &R(NBA97_MATCH_INITIALIZE_V0), 480);
        R(NBA97_MATCH_INITIALIZE_V0) = branch_value;
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 120);
        TRY(decide_zero(run, &branch_value, UINT32_C(0x8002a3cc),
            &branch));
        if (branch) {
            set_known(&R(NBA97_MATCH_INITIALIZE_AT),
                UINT32_C(0x80100000));
            TRY(write_value(run, AUDIO_TIMER, 2, UINT32_C(0x8002a3d8),
                &R(NBA97_MATCH_INITIALIZE_V0)));
        }
    }

    /* 0x8002A3DC..0x8002A418: subtract the live s1, store the wrapped low
     * half, and clear a0 in the BGEZ delay before an optional timeout call. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_V1) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffda0e));
    TRY(read_value(run, AUDIO_TIMER, 2, UINT32_C(0x8002a3e4), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
    R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_GAME_MATCH_CLOCKS_S1));
    TRY(write_value(run, R(NBA97_MATCH_INITIALIZE_V1).word, 2,
        UINT32_C(0x8002a3f0), &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = sll16(
        R(NBA97_MATCH_INITIALIZE_V0));
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
    TRY(decide_nonnegative(run, &branch_value, UINT32_C(0x8002a3f8),
        &branch));
    if (branch)
        goto restore_registers;
    prepare_jal(run, UINT32_C(0x8002a400));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 120);
    TRY(invoke_prepared(run, UINT32_C(0x8002a400),
        UINT32_C(0x8002a0a8),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A0A8, 2));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, AUDIO_MODE, 2, UINT32_C(0x8002a410),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    goto restore_registers;

mode_2:
    /* 0x8002A41C..0x8002A448: pass the live result handle to AD9FC. The
     * BGTZ delay always sets a0=9; nonpositive results clear callback-live
     * *s0, call 9DC10(9,0,0), then call 9F8D8. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    TRY(read_value(run, AUDIO_RESULT, 4, UINT32_C(0x8002a420),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    TRY(call_nop(run, UINT32_C(0x8002a424), UINT32_C(0x800ad9fc),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC, 1));
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 9);
    TRY(decide_positive(run, &branch_value, UINT32_C(0x8002a42c),
        &branch));
    if (branch)
        goto restore_registers;
    set_known(&value, 0);
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_S0), 0,
        UINT32_C(0x8002a434), &address));
    TRY(write_value(run, address, 2, UINT32_C(0x8002a434), &value));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
    prepare_jal(run, UINT32_C(0x8002a43c));
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0);
    TRY(invoke_prepared(run, UINT32_C(0x8002a43c),
        UINT32_C(0x8009dc10),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009DC10, 3));
    TRY(call_nop(run, UINT32_C(0x8002a444), UINT32_C(0x8009f8d8),
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009F8D8, 0));

restore_registers:
    /* 0x8002A44C..0x8002A460: all three reloads use callback-live sp; the
     * wrapping ADDIU precedes JR, whose unknown target stops after its NOP. */
    TRY(restore(run, UINT32_C(0x8002a44c), 0x18u,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x8002a450), 0x14u,
        NBA97_GAME_MATCH_CLOCKS_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x8002a454), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    publish(run);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8002a45c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
