#include "game_ball_acquire.h"

#include <limits.h>
#include <string.h>

typedef struct Run {
    Nba97GameBallAcquireContext* context;
    Nba97GameBallAcquireProgress* out;
    Nba97GameBallAcquireMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define TRY(x) do { int status_ = (x); if (status_ != NBA97_TEXT_COMPLETE) return status_; } while (0)

static void publish(Run* run) { run->out->machine = run->machine; }
static void stop(Run* run, uint32_t pc, uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}
static void known(Nba97GameBallAcquireWord* out, uint32_t word) {
    out->word = word;
    out->known_mask = 0x0fu;
}
static int machine_valid(const Nba97GameBallAcquireMachine* machine) {
    unsigned i;
    if (machine->registers.gpr[0].word != 0 ||
        machine->registers.gpr[0].known_mask != 0x0fu ||
        machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine->registers.gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}
static int begin(Nba97GameBallAcquireContext* context,
    Nba97GameBallAcquireProgress* out, Run* run) {
    size_t i, j;
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

/* Byte-domain carry propagation keeps every source-provable result byte. */
static Nba97GameBallAcquireWord add_word(Nba97GameBallAcquireWord a,
    Nba97GameBallAcquireWord b) {
    Nba97GameBallAcquireWord r;
    unsigned carry_mask = 1u, byte;
    r.word = a.word + b.word;
    r.known_mask = 0;
    if (a.known_mask == 0x0fu && b.known_mask == 0x0fu) {
        r.known_mask = 0x0fu;
        return r;
    }
    for (byte = 0; byte < 4; ++byte) {
        unsigned next = 0, first_value = 0, first = 1, invariant = 1;
        unsigned as = (a.known_mask & (1u << byte)) ?
            ((a.word >> (8u * byte)) & 255u) : 0u;
        unsigned ae = (a.known_mask & (1u << byte)) ? as : 255u;
        unsigned bs = (b.known_mask & (1u << byte)) ?
            ((b.word >> (8u * byte)) & 255u) : 0u;
        unsigned be = (b.known_mask & (1u << byte)) ? bs : 255u;
        unsigned carry, x, y;
        for (carry = 0; carry < 2; ++carry) if (carry_mask & (1u << carry))
            for (x = as; x <= ae; ++x) for (y = bs; y <= be; ++y) {
                unsigned sum = x + y + carry, value = sum & 255u;
                next |= 1u << (sum >> 8u);
                if (first) { first_value = value; first = 0; }
                else if (value != first_value) invariant = 0;
            }
        if (invariant) r.known_mask = (uint8_t)(r.known_mask | (1u << byte));
        carry_mask = next;
    }
    return r;
}
static Nba97GameBallAcquireWord add_constant(Nba97GameBallAcquireWord a,
    uint32_t value) {
    Nba97GameBallAcquireWord b;
    known(&b, value);
    return add_word(a, b);
}
static uint32_t mask_for(unsigned width) {
    return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}
static uint8_t known_for(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}
static int spend(Run* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}
static void journal(Run* run, uint8_t kind, uint32_t pc, uint32_t address,
    unsigned width, Nba97GameBallAcquireWord value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameBallAcquireAccess* e = &run->context->access_journal[index];
        e->pc = pc; e->address = address; e->value = value.word & mask_for(width);
        e->operation = run->out->operations; e->width = (uint8_t)width;
        e->known_mask = (uint8_t)(value.known_mask & known_for(width));
        e->kind = kind;
    }
}
static int locate(Run* run, uint32_t pc, uint32_t address, unsigned width,
    uint8_t** data, uint8_t** provenance) {
    size_t i, j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (address & (width - 1u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        uint64_t offset = (uint64_t)address - region->base;
        if (address < region->base || offset > region->size ||
            width > region->size - (size_t)offset)
            continue;
        *data = region->data + (size_t)offset;
        *provenance = region->known ? region->known + (size_t)offset : 0;
        if (*provenance)
            for (j = 0; j < width; ++j)
                if ((*provenance)[j] > 1)
                    return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_mem(Run* run, uint32_t pc, uint32_t address, unsigned width,
    Nba97GameBallAcquireWord* out) {
    uint8_t *data, *provenance;
    unsigned i;
    out->word = 0; out->known_mask = 0;
    TRY(locate(run, pc, address, width, &data, &provenance));
    for (i = 0; i < width; ++i) {
        out->word |= (uint32_t)data[i] << (8u * i);
        if (!provenance || provenance[i])
            out->known_mask = (uint8_t)(out->known_mask | (1u << i));
    }
    ++run->out->reads;
    journal(run, NBA97_GAME_BALL_ACQUIRE_READ, pc, address, width, *out);
    return NBA97_TEXT_COMPLETE;
}
static int write_mem(Run* run, uint32_t pc, uint32_t address, unsigned width,
    Nba97GameBallAcquireWord value) {
    uint8_t *data, *provenance;
    unsigned i;
    TRY(locate(run, pc, address, width, &data, &provenance));
    if (!provenance &&
        (value.known_mask & known_for(width)) != known_for(width))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value.word >> (8u * i));
        if (provenance) provenance[i] = (uint8_t)((value.known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_BALL_ACQUIRE_STORE, pc, address, width, value);
    return NBA97_TEXT_COMPLETE;
}
static int effective(Run* run, uint32_t pc, Nba97GameBallAcquireWord base,
    uint32_t offset, uint32_t* address) {
    *address = base.word + offset;
    if (base.known_mask != 0x0fu) {
        stop(run, pc, *address, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}
static Nba97GameBallAcquireWord zero_extend(Nba97GameBallAcquireWord value,
    unsigned width) {
    value.word &= mask_for(width);
    value.known_mask = (uint8_t)((value.known_mask & known_for(width)) |
        (0x0fu & ~known_for(width)));
    return value;
}
static Nba97GameBallAcquireWord sign_extend(Nba97GameBallAcquireWord value,
    unsigned width) {
    uint32_t sign = UINT32_C(1) << (width * 8u - 1u);
    value.word &= mask_for(width);
    if (value.word & sign) value.word |= ~mask_for(width);
    if (value.known_mask & (1u << (width - 1u)))
        value.known_mask = (uint8_t)(value.known_mask |
            (0x0fu & ~known_for(width)));
    return value;
}
static int load_abs(Run* run, uint32_t pc, unsigned reg, uint32_t address,
    unsigned width, int sign) {
    Nba97GameBallAcquireWord value;
    TRY(read_mem(run, pc, address, width, &value));
    R(reg) = sign ? sign_extend(value, width) : zero_extend(value, width);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}
static int load_at(Run* run, uint32_t pc, unsigned reg,
    Nba97GameBallAcquireWord base, uint32_t offset, unsigned width, int sign) {
    uint32_t address;
    TRY(effective(run, pc, base, offset, &address));
    return load_abs(run, pc, reg, address, width, sign);
}
static int store_abs(Run* run, uint32_t pc, uint32_t address, unsigned width,
    Nba97GameBallAcquireWord value) {
    return write_mem(run, pc, address, width, value);
}
static int store_at(Run* run, uint32_t pc, Nba97GameBallAcquireWord base,
    uint32_t offset, unsigned width, Nba97GameBallAcquireWord value) {
    uint32_t address;
    TRY(effective(run, pc, base, offset, &address));
    return store_abs(run, pc, address, width, value);
}
static int require_full(Run* run, uint32_t pc,
    Nba97GameBallAcquireWord value, uint32_t* word) {
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *word = value.word;
    return NBA97_TEXT_COMPLETE;
}
static int decide_equal(Run* run, uint32_t pc,
    Nba97GameBallAcquireWord a, Nba97GameBallAcquireWord b, int* equal) {
    unsigned byte;
    for (byte = 0; byte < 4; ++byte)
        if ((a.known_mask & b.known_mask & (1u << byte)) &&
            (((a.word ^ b.word) >> (8u * byte)) & 255u)) {
            *equal = 0; return NBA97_TEXT_COMPLETE;
        }
    if ((a.known_mask & b.known_mask) == 0x0fu) {
        *equal = 1; return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}
static void unsigned_bounds(Nba97GameBallAcquireWord value,
    uint32_t* low, uint32_t* high) {
    unsigned i;
    *low = *high = 0;
    for (i = 0; i < 4; ++i) {
        uint32_t byte = (value.word >> (8u * i)) & 255u;
        *low |= ((value.known_mask & (1u << i)) ? byte : 0u) << (8u * i);
        *high |= ((value.known_mask & (1u << i)) ? byte : 255u) << (8u * i);
    }
}
static int32_t signed_word(uint32_t value) {
    if (value <= (uint32_t)INT32_MAX) return (int32_t)value;
    return -1 - (int32_t)~value;
}
static Nba97GameBallAcquireWord comparison_word(int actual, int determined) {
    Nba97GameBallAcquireWord result;
    result.word = actual ? 1u : 0u;
    result.known_mask = (uint8_t)(determined ? 0x0fu : 0x0eu);
    return result;
}
static Nba97GameBallAcquireWord unsigned_lt_word(
    Nba97GameBallAcquireWord value, uint32_t limit) {
    uint32_t low, high;
    unsigned_bounds(value, &low, &high);
    return comparison_word(value.word < limit,
        high < limit || low >= limit);
}
static Nba97GameBallAcquireWord unsigned_words_lt_word(
    Nba97GameBallAcquireWord left, Nba97GameBallAcquireWord right) {
    uint32_t ll, lh, rl, rh;
    unsigned_bounds(left, &ll, &lh); unsigned_bounds(right, &rl, &rh);
    return comparison_word(left.word < right.word, lh < rl || ll >= rh);
}
static Nba97GameBallAcquireWord signed_lt_word(
    Nba97GameBallAcquireWord value, int32_t limit) {
    uint32_t low, high;
    int determined = 0;
    if (value.known_mask & 8u) {
        unsigned_bounds(value, &low, &high);
        if (value.word & UINT32_C(0x80000000)) {
            low |= UINT32_C(0x80000000); high |= UINT32_C(0x80000000);
        } else {
            low &= UINT32_C(0x7fffffff); high &= UINT32_C(0x7fffffff);
        }
        determined = signed_word(high) < limit || signed_word(low) >= limit;
    }
    return comparison_word(signed_word(value.word) < limit, determined);
}
static int decide_negative(Run* run, uint32_t pc,
    Nba97GameBallAcquireWord value, int* result) {
    if (!(value.known_mask & 8u)) {
        stop(run, pc, 0, 0); return NBA97_TEXT_UNKNOWN;
    }
    *result = (value.word & UINT32_C(0x80000000)) != 0;
    return NBA97_TEXT_COMPLETE;
}
static Nba97GameBallAcquireWord and_constant(
    Nba97GameBallAcquireWord value, uint32_t mask) {
    unsigned i;
    value.word &= mask;
    for (i = 0; i < 4; ++i)
        if (((mask >> (8u * i)) & 255u) == 0)
            value.known_mask = (uint8_t)(value.known_mask | (1u << i));
    return value;
}
static Nba97GameBallAcquireWord or_constant(
    Nba97GameBallAcquireWord value, uint32_t bits) {
    unsigned i;
    value.word |= bits;
    for (i = 0; i < 4; ++i)
        if (((bits >> (8u * i)) & 255u) == 255u)
            value.known_mask = (uint8_t)(value.known_mask | (1u << i));
    return value;
}
static Nba97GameBallAcquireWord shift_left2(Nba97GameBallAcquireWord value) {
    Nba97GameBallAcquireWord r;
    r.word = value.word << 2;
    r.known_mask = 0;
    if (value.known_mask & 1u) r.known_mask |= 1u;
    if ((value.known_mask & 3u) == 3u) r.known_mask |= 2u;
    if ((value.known_mask & 6u) == 6u) r.known_mask |= 4u;
    if ((value.known_mask & 12u) == 12u) r.known_mask |= 8u;
    return r;
}
static Nba97GameBallAcquireWord arithmetic_half16(
    Nba97GameBallAcquireWord value) {
    Nba97GameBallAcquireWord r;
    uint32_t bits = value.word & 0xffffu;
    int32_t signed_value = bits < 0x8000u ?
        (int32_t)bits : (int32_t)bits - 65536;
    int32_t half = signed_value >= 0 ?
        signed_value / 2 : (signed_value - 1) / 2;
    r.word = (uint32_t)half;
    r.known_mask = (uint8_t)(((value.known_mask & 3u) == 3u ? 1u : 0u) |
        ((value.known_mask & 2u) ? 0x0eu : 0u));
    return r;
}
static int negative_with_sll2_delay(Run* run, uint32_t pc,
    Nba97GameBallAcquireWord source, int* negative) {
    int status = decide_negative(run, pc, source, negative);
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left2(source);
    publish(run);
    return status;
}
static int decide_positive(Run* run, uint32_t pc,
    Nba97GameBallAcquireWord value, int* result) {
    int negative, equal;
    Nba97GameBallAcquireWord zero;
    known(&zero, 0);
    TRY(decide_negative(run, pc, value, &negative));
    if (negative) { *result = 0; return NBA97_TEXT_COMPLETE; }
    {
        int status = decide_equal(run, pc, value, zero, &equal);
        if (status == NBA97_TEXT_COMPLETE) {
            *result = !equal;
            return NBA97_TEXT_COMPLETE;
        }
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}
static int invoke(Run* run, uint32_t pc, uint32_t entry, uint8_t kind,
    uint8_t argument_count) {
    Nba97GameBallAcquireEvent event;
    size_t invocation;
    stop(run, pc, 0, entry);
    known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    publish(run);
    TRY(spend(run));
    invocation = ++run->out->call_count[kind];
    event.pc = pc; event.delay_slot_pc = pc + 4u; event.entry = entry;
    event.operation = run->out->operations; event.invocation = invocation;
    event.kind = kind; event.argument_count = argument_count;
    publish(run);
    if (!run->context->io || run->context->io(run->context->user,
            &run->context->memory, &event, &run->machine) != 1) {
        publish(run); return NBA97_TEXT_IO_REFUSED;
    }
    publish(run);
    if (!machine_valid(&run->machine))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int cleanup(Run* run) {
    Nba97GameBallAcquireWord zero, one;
    known(&zero, 0); known(&one, 1);
    /* 0x8005D780: cache the signed return halfword before cleanup writes. */
    known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d784), NBA97_MATCH_INITIALIZE_V0,
        UINT32_C(0x800fdb96), 2, 1));
    known(&R(NBA97_MATCH_INITIALIZE_V1), 1);
    known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d790), UINT32_C(0x800fdbca), 2, one));
    TRY(store_abs(run, UINT32_C(0x8005d798), UINT32_C(0x800fdbd4), 2, zero));
    TRY(store_abs(run, UINT32_C(0x8005d7a0), UINT32_C(0x800fdbb0), 2, zero));
    TRY(store_abs(run, UINT32_C(0x8005d7a8), UINT32_C(0x800fdbda), 2, zero));
    TRY(store_abs(run, UINT32_C(0x8005d7b0), UINT32_C(0x800fdbd8), 2, zero));
    return NBA97_TEXT_COMPLETE;
}

static int finish_frame(Run* run) {
    uint32_t sp;
    TRY(require_full(run, UINT32_C(0x8005d9d0),
        R(NBA97_MATCH_INITIALIZE_SP), &sp));
    TRY(load_abs(run, UINT32_C(0x8005d9d0), NBA97_MATCH_INITIALIZE_RA,
        sp + 0x28u, 4, 0)); run->out->restored_return_address = R(31);
    TRY(load_abs(run, UINT32_C(0x8005d9d4), 19, sp + 0x24u, 4, 0));
    run->out->restored_s3 = R(19);
    TRY(load_abs(run, UINT32_C(0x8005d9d8), 18, sp + 0x20u, 4, 0));
    run->out->restored_s2 = R(18);
    TRY(load_abs(run, UINT32_C(0x8005d9dc), 17, sp + 0x1cu, 4, 0));
    run->out->restored_s1 = R(17);
    TRY(load_abs(run, UINT32_C(0x8005d9e0), 16, sp + 0x18u, 4, 0));
    run->out->restored_s0 = R(16);
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x30u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8005d9e8), R(31).word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    run->out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}

static int stat_call(Run* run, uint32_t pc, uint32_t code,
    Nba97GameBallAcquireWord actor_id) {
    R(NBA97_MATCH_INITIALIZE_A1) = actor_id;
    known(&R(NBA97_MATCH_INITIALIZE_A0), code); /* JAL delay slot. */
    return invoke(run, pc, UINT32_C(0x80035318),
        NBA97_GAME_BALL_ACQUIRE_CHILD_80035318, 2);
}

static int increment_controller(Run* run, uint32_t pointer_pc,
    uint32_t stat_pc, uint32_t store_pc, Nba97GameBallAcquireWord base,
    uint32_t pointer_offset, uint32_t stat_offset) {
    uint32_t address;
    TRY(load_at(run, pointer_pc, NBA97_MATCH_INITIALIZE_V1,
        base, pointer_offset, 4, 0));
    TRY(effective(run, stat_pc, R(3), stat_offset, &address));
    TRY(load_abs(run, stat_pc, NBA97_MATCH_INITIALIZE_V0, address, 2, 0));
    R(2) = add_constant(R(2), 1u);
    return store_abs(run, store_pc, address, 2, R(2));
}

static int execute(Run* run) {
    Nba97GameBallAcquireWord zero, minus_one;
    uint32_t sp, team_base, stats;
    Nba97GameBallAcquireWord motion, flag;
    int branch, negative;
    known(&zero, 0); known(&minus_one, UINT32_MAX);

    /* 0x8005D140: pre-frame gate read, then the live five-word frame. */
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d144), 2, UINT32_C(0x800fa034), 4, 0));
    R(29) = add_constant(R(29), UINT32_C(0xffffffd0));
    TRY(require_full(run, UINT32_C(0x8005d14c), R(29), &sp));
    run->out->frame_stack_pointer = sp;
    TRY(store_abs(run, UINT32_C(0x8005d14c), sp + 0x1cu, 4, R(17)));
    R(17) = R(4);
    TRY(store_abs(run, UINT32_C(0x8005d154), sp + 0x28u, 4, R(31)));
    TRY(store_abs(run, UINT32_C(0x8005d158), sp + 0x24u, 4, R(19)));
    TRY(store_abs(run, UINT32_C(0x8005d15c), sp + 0x20u, 4, R(18)));
    TRY(store_abs(run, UINT32_C(0x8005d164), sp + 0x18u, 4, R(16)));
    TRY(decide_negative(run, UINT32_C(0x8005d160), R(2), &negative));
    if (!negative) {
        known(&R(3), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d16c), 3, UINT32_C(0x800fdb90), 2, 1));
        known(&R(2), 0x82u);
        TRY(decide_equal(run, UINT32_C(0x8005d174), R(3), R(2), &branch));
        if (branch) {
            known(&R(2), UINT32_C(0x80100000));
            TRY(load_abs(run, UINT32_C(0x8005d180), 2,
                UINT32_C(0x800fe882), 2, 1));
            TRY(decide_equal(run, UINT32_C(0x8005d188), R(2), zero, &branch));
            if (branch) {
                known(&R(16), UINT32_C(0x80020000));
                TRY(load_abs(run, UINT32_C(0x8005d194), 16,
                    UINT32_C(0x80021d95), 1, 0));
                TRY(decide_equal(run, UINT32_C(0x8005d19c), R(16), zero, &branch));
                if (!branch) {
                    R(16) = and_constant(R(16), 255u); /* call delay */
                    TRY(invoke(run, UINT32_C(0x8005d1a4),
                        UINT32_C(0x8002ab70),
                        NBA97_GAME_BALL_ACQUIRE_CHILD_8002AB70, 0));
                    R(2) = and_constant(R(2), 7u);
                    R(2) = unsigned_words_lt_word(R(2), R(16));
                    motion = R(2);
                    known(&R(2), 1u); /* BEQ delay */
                    TRY(decide_equal(run, UINT32_C(0x8005d1b4),
                        motion, zero, &branch));
                    if (!branch) {
                        known(&R(1), UINT32_C(0x80100000));
                        TRY(store_abs(run, UINT32_C(0x8005d1c0),
                            UINT32_C(0x800fa038), 2, R(2)));
                        run->out->random_rule_set = 1;
                    }
                }
            }
        }
    }

    /* 0x8005D1C4: choose team storage using unsigned actor team byte. */
    TRY(load_at(run, UINT32_C(0x8005d1c4), 2, R(17), 0xd9u, 1, 0));
    known(&R(16), UINT32_C(0x8001edf4));
    TRY(decide_equal(run, UINT32_C(0x8005d1d0), R(2), zero, &branch));
    if (!branch) R(16) = add_constant(R(16), 0xc4u);
    TRY(require_full(run, UINT32_C(0x8005d1dc), R(16), &team_base));
    known(&R(3), UINT32_C(0x800fe8e2));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d1e8), UINT32_C(0x800fe90e), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d1f0), UINT32_C(0x800fdbc8), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d1f8), UINT32_C(0x800fe90a), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d200), UINT32_C(0x800fdbb2), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d208), UINT32_C(0x800fdbdc), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d210), UINT32_C(0x800fdbe8), 2, zero));
    TRY(store_abs(run, UINT32_C(0x8005d214), UINT32_C(0x800fe8e2), 2, zero));

    TRY(load_at(run, UINT32_C(0x8005d218), 2, R(17), 0xa0u, 2, 1));
    R(2) = signed_lt_word(R(2), 385);
    TRY(decide_equal(run, UINT32_C(0x8005d224), R(2), zero, &branch));
    if (!branch) {
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d230), 2,
            UINT32_C(0x800fdb90), 2, 1));
        R(2) = signed_lt_word(R(2), 128);
        TRY(decide_equal(run, UINT32_C(0x8005d23c), R(2), zero, &branch));
        if (!branch) {
            TRY(load_at(run, UINT32_C(0x8005d244), 2, R(17), 0x10u, 4, 0));
            motion = R(2);
            known(&R(2), 100u); /* BNE delay, even when taken */
            TRY(decide_equal(run, UINT32_C(0x8005d24c), motion, zero, &branch));
            if (branch) {
                TRY(store_at(run, UINT32_C(0x8005d254), R(17), 0xb8u, 2, R(2)));
                known(&R(2), 30u);
                TRY(store_at(run, UINT32_C(0x8005d25c), R(17), 0xe8u, 2, R(2)));
                known(&R(2), UINT32_MAX);
                TRY(store_at(run, UINT32_C(0x8005d264), R(17), 0x16u, 2, zero));
                TRY(store_at(run, UINT32_C(0x8005d268), R(17), 0x14u, 2, zero));
                TRY(store_abs(run, UINT32_C(0x8005d26c), UINT32_C(0x800fe8e2), 2, R(2)));
                run->out->grounded_reset = 1;
            }
        }
    }

    /* 0x8005D270: publish ownership before consuming descriptor+0x0D. */
    TRY(load_at(run, UINT32_C(0x8005d270), 2, R(17), 0, 4, 0));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d278), UINT32_C(0x800fdc34), 4, R(17)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d280), UINT32_C(0x800fdc38), 4, R(16)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d288), UINT32_C(0x800fdbcc), 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d28c), 2, R(17), 0x20u, 4, 0));
    TRY(load_at(run, UINT32_C(0x8005d294), 2, R(2), 0x0du, 1, 0));
    TRY(store_at(run, UINT32_C(0x8005d2a0), R(17), 0xcau, 1, R(2)));
    TRY(decide_equal(run, UINT32_C(0x8005d29c), R(2), zero, &branch));
    TRY(load_at(run, branch ? UINT32_C(0x8005d2b0) : UINT32_C(0x8005d2a4),
        2, R(17), 0x9au, 2, 0));
    R(2) = branch ? and_constant(R(2), 0xfffcu) : or_constant(R(2), 3u);
    TRY(store_at(run, UINT32_C(0x8005d2bc), R(17), 0x9au, 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d2c0), 2, R(17), 8u, 4, 0));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d2c8), UINT32_C(0x800fdbf4), 4, R(2)));
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d2d0), 2, UINT32_C(0x800fdb90), 2, 1));
    TRY(load_at(run, UINT32_C(0x8005d2d4), 3, R(17), 0x0cu, 4, 0));
    R(2) = signed_lt_word(R(2), 128);
    motion = R(2);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d2e0), UINT32_C(0x800fdbf8), 4, R(3)));
    known(&R(2), UINT32_MAX); /* branch delay */
    TRY(decide_equal(run, UINT32_C(0x8005d2e4), motion, zero, &branch));
    if (!branch) {
        TRY(load_abs(run, UINT32_C(0x8005d2ec), 2, team_base + 0x52u, 2, 0));
        TRY(load_abs(run, UINT32_C(0x8005d2f0), 3, team_base + 0x54u, 2, 0));
        TRY(store_abs(run, UINT32_C(0x8005d2f4), team_base + 0x56u, 2, R(2)));
        TRY(store_abs(run, UINT32_C(0x8005d2fc), team_base + 0x58u, 2, R(3)));
    } else {
        TRY(store_abs(run, UINT32_C(0x8005d300), team_base + 0x58u, 2, R(2)));
        TRY(store_abs(run, UINT32_C(0x8005d304), team_base + 0x56u, 2, R(2)));
    }
    TRY(load_at(run, UINT32_C(0x8005d308), 2, R(17), 0, 4, 0));
    TRY(store_abs(run, UINT32_C(0x8005d310), team_base + 0x52u, 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d314), 2, R(17), 4u, 2, 0));
    TRY(store_abs(run, UINT32_C(0x8005d31c), team_base + 0x54u, 2, R(2)));
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d324), 2, UINT32_C(0x800fdb58), 4, 0));
    TRY(store_abs(run, UINT32_C(0x8005d32c), team_base + 0x5au, 2, R(2)));
    known(&R(2), UINT32_MAX);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d338), UINT32_C(0x800fe8c2), 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d340), UINT32_C(0x800fe874), 2, R(2)));
    known(&R(2), 300u);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d34c), UINT32_C(0x800fdba8), 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d354), UINT32_C(0x800fe892), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d35c), UINT32_C(0x800fdbea), 2, zero));
    TRY(load_at(run, UINT32_C(0x8005d360), 3, R(17), 0x1au, 1, 0));
    known(&R(2), 15u);
    TRY(store_at(run, UINT32_C(0x8005d36c), R(17), 0x98u, 2, zero));
    TRY(decide_equal(run, UINT32_C(0x8005d368), R(3), R(2), &branch));
    if (!branch) {
        known(&R(2), 19u);
        motion = R(2);
        known(&R(2), 11u); /* BEQ delay */
        TRY(decide_equal(run, UINT32_C(0x8005d374), R(3), motion, &branch));
        if (!branch) {
            TRY(store_at(run, UINT32_C(0x8005d37c), R(17), 0x1au, 1, R(2)));
            TRY(store_at(run, UINT32_C(0x8005d380), R(17), 0xdau, 1, zero));
            TRY(store_at(run, UINT32_C(0x8005d384), R(17), 0xb6u, 2, zero));
            known(&R(2), UINT32_C(0x80100000));
            TRY(load_abs(run, UINT32_C(0x8005d38c), 2, UINT32_C(0x800fa038), 2, 1));
            flag = R(2);
            known(&R(2), UINT32_MAX); /* branch delay */
            TRY(decide_equal(run, UINT32_C(0x8005d394), flag, zero, &branch));
            if (branch) {
                known(&R(1), UINT32_C(0x80100000));
                TRY(store_abs(run, UINT32_C(0x8005d3a0),
                    UINT32_C(0x800fa034), 4, R(2)));
            }
        }
    }

    known(&R(3), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d3a8), 3, UINT32_C(0x800fdb94), 2, 1));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d3b0), UINT32_C(0x800fdbd6), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d3b8), UINT32_C(0x800fe898), 4, zero));
    TRY(load_at(run, UINT32_C(0x8005d3bc), 2, R(17), 0xd9u, 1, 0));
    R(4) = R(3); /* branch delay */
    TRY(decide_equal(run, UINT32_C(0x8005d3c4), R(2), R(3), &branch));
    if (branch) goto same_team;

    /* 0x8005D3CC: possession changes to the actor's team. */
    run->out->possession_changed = 1;
    known(&R(19), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d3d0), 19, UINT32_C(0x800fdb96), 2, 1));
    known(&R(2), 600u);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d3dc), UINT32_C(0x800fdbaa), 2, R(2)));
    known(&R(2), UINT32_MAX);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d3e8), UINT32_C(0x800fdbac), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d3f0), UINT32_C(0x800fe8e0), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d3f8), UINT32_C(0x800fdb96), 2, zero));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d400), UINT32_C(0x800fdb94), 2, zero));
    TRY(store_abs(run, UINT32_C(0x8005d404), team_base + 0x58u, 2, R(2)));
    TRY(store_abs(run, UINT32_C(0x8005d408), team_base + 0x56u, 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d410), UINT32_C(0x800fe87e), 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d418), UINT32_C(0x800fe87c), 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d420), UINT32_C(0x800fe87a), 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d428), UINT32_C(0x800fe8a8), 2, R(2)));
    known(&R(2), 3u);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d434), UINT32_C(0x800fe86e), 2, R(2)));
    known(&R(2), 1440u);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d440), UINT32_C(0x800fdba4), 4, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d444), 2, R(17), 0xd9u, 1, 0));
    known(&R(3), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d44c), 3, UINT32_C(0x800fdc40), 4, 0));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d454), UINT32_C(0x800fdb96), 2, R(2)));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d45c), UINT32_C(0x800fdb94), 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d460), 4, R(3), 4u, 4, 0));
    TRY(invoke(run, UINT32_C(0x8005d464), UINT32_C(0x80072c40),
        NBA97_GAME_BALL_ACQUIRE_CHILD_80072C40, 1));
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d470), 2, UINT32_C(0x800fdb90), 2, 1));
    R(2) = signed_lt_word(R(2), 128);
    TRY(decide_equal(run, UINT32_C(0x8005d47c), R(2), zero, &branch));
    if (!branch) {
        TRY(load_at(run, UINT32_C(0x8005d484), 2, R(17), 0, 4, 0));
        R(2) = signed_lt_word(R(2), 5);
        TRY(decide_equal(run, UINT32_C(0x8005d490), R(2), zero, &branch));
        known(&R(4), branch ? 10000u : 20000u); /* JAL delay */
        TRY(invoke(run, branch ? UINT32_C(0x8005d498) : UINT32_C(0x8005d4a8),
            UINT32_C(0x800295c8), NBA97_GAME_BALL_ACQUIRE_CHILD_800295C8, 1));
        known(&R(4), branch ? 1u : 2u);
        TRY(invoke(run, UINT32_C(0x8005d4b4), UINT32_C(0x80029590),
            NBA97_GAME_BALL_ACQUIRE_CHILD_80029590, 1));
    }
    known(&R(18), UINT32_C(0x800fdbca));
    TRY(load_abs(run, UINT32_C(0x8005d4c4), 2, UINT32_C(0x800fdbca), 2, 1));
    TRY(decide_equal(run, UINT32_C(0x8005d4cc), R(2), zero, &branch));
    if (branch) goto common;
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d4d8), 2, UINT32_C(0x800fdbd2), 2, 1));
    TRY(decide_negative(run, UINT32_C(0x8005d4e0), R(2), &negative));
    if (negative) {
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d4ec), 2, UINT32_C(0x800fdb90), 2, 1));
        R(2) = signed_lt_word(R(2), 128);
        TRY(decide_equal(run, UINT32_C(0x8005d4f8), R(2), zero, &branch));
        if (branch) goto common;
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d504), 2, UINT32_C(0x800fdbb0), 2, 0));
        TRY(decide_equal(run, UINT32_C(0x8005d50c), R(2), zero, &branch));
        if (!branch) goto common;
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d518), 2, UINT32_C(0x800fdbd4), 2, 1));
        TRY(decide_equal(run, UINT32_C(0x8005d520), R(2), zero, &branch));
        if (branch) goto d4_zero;
        TRY(decide_positive(run, UINT32_C(0x8005d528), R(2), &branch));
        if (branch) goto general_change_stats;
        TRY(load_at(run, UINT32_C(0x8005d530), 2, R(17), 0xdfu, 1, 0));
        TRY(load_at(run, UINT32_C(0x8005d534), 4, R(17), 0x1cu, 4, 0));
        R(2) = add_constant(R(2), 1u);
        TRY(store_at(run, UINT32_C(0x8005d53c), R(17), 0xdfu, 1, R(2)));
        TRY(require_full(run, UINT32_C(0x8005d540), R(4), &stats));
        TRY(load_abs(run, UINT32_C(0x8005d540), 3, stats + 0x0cu, 2, 0));
        R(2) = unsigned_lt_word(R(3), 999u); motion = R(2);
        R(2) = add_constant(R(3), 1u);
        TRY(decide_equal(run, UINT32_C(0x8005d54c), motion, zero, &branch));
        if (!branch) {
            TRY(store_abs(run, UINT32_C(0x8005d554), stats + 0x0cu, 2, R(2)));
            TRY(load_at(run, UINT32_C(0x8005d558), 5, R(17), 0, 2, 1));
            TRY(stat_call(run, UINT32_C(0x8005d55c), 17u, R(5)));
        }
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d568), 2, UINT32_C(0x800fdb84), 2, 1));
        TRY(decide_equal(run, UINT32_C(0x8005d570), R(2), zero, &branch)); if (!branch) goto common;
        TRY(load_at(run, UINT32_C(0x8005d578), 2, R(17), 4u, 2, 1));
        {
            Nba97GameBallAcquireWord index = R(2);
            TRY(negative_with_sll2_delay(run, UINT32_C(0x8005d580),
                index, &negative));
        }
        if (negative) goto common;
        R(2) = add_word(R(18), R(2));
        TRY(increment_controller(run, UINT32_C(0x8005d58c), UINT32_C(0x8005d594),
            UINT32_C(0x8005d5a4), R(2), 0x86u, 0x0cu));
        goto common;
d4_zero:
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d5ac), 2, UINT32_C(0x800fdbd8), 2, 1));
        TRY(decide_equal(run, UINT32_C(0x8005d5b4), R(2), zero, &branch));
        if (branch) goto general_change_stats;
        TRY(load_at(run, UINT32_C(0x8005d5bc), 2, R(17), 0xdfu, 1, 0));
        TRY(load_at(run, UINT32_C(0x8005d5c0), 4, R(17), 0x1cu, 4, 0));
        R(2) = add_constant(R(2), 1u);
        TRY(store_at(run, UINT32_C(0x8005d5c8), R(17), 0xdfu, 1, R(2)));
        TRY(require_full(run, UINT32_C(0x8005d5cc), R(4), &stats));
        TRY(load_abs(run, UINT32_C(0x8005d5cc), 3, stats + 0x16u, 2, 0));
        R(2) = unsigned_lt_word(R(3), 999u); motion = R(2);
        R(2) = add_constant(R(3), 1u);
        TRY(decide_equal(run, UINT32_C(0x8005d5d8), motion, zero, &branch));
        if (!branch) {
            TRY(store_abs(run, UINT32_C(0x8005d5e0), stats + 0x16u, 2, R(2)));
            TRY(load_at(run, UINT32_C(0x8005d5e4), 5, R(17), 0, 2, 1));
            TRY(stat_call(run, UINT32_C(0x8005d5e8), 13u, R(5)));
        }
        known(&R(2), UINT32_C(0x80100000));
        TRY(load_abs(run, UINT32_C(0x8005d5f4), 2, UINT32_C(0x800fdb84), 2, 1));
        TRY(decide_equal(run, UINT32_C(0x8005d5fc), R(2), zero, &branch)); if (!branch) goto common;
        TRY(load_at(run, UINT32_C(0x8005d604), 2, R(17), 4u, 2, 1));
        {
            Nba97GameBallAcquireWord index = R(2);
            TRY(negative_with_sll2_delay(run, UINT32_C(0x8005d60c),
                index, &negative));
            if (negative) goto common;
        }
        R(2) = add_word(R(18), R(2));
        TRY(increment_controller(run, UINT32_C(0x8005d618), UINT32_C(0x8005d620),
            UINT32_C(0x8005d630), R(2), 0x86u, 0x16u));
        goto common;
    }

general_change_stats:
    known(&R(18), UINT32_C(0x800fdb96));
    TRY(load_abs(run, UINT32_C(0x8005d63c), 2, UINT32_C(0x800fdb96), 2, 1));
    TRY(decide_equal(run, UINT32_C(0x8005d644), R(19), R(2), &branch));
    if (branch) goto old_team_equal;
    TRY(load_at(run, UINT32_C(0x8005d64c), 2, R(17), 0xdfu, 1, 0));
    R(2) = add_constant(R(2), 1u);
    TRY(store_at(run, UINT32_C(0x8005d658), R(17), 0xdfu, 1, R(2)));
    known(&R(2), 2u);
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d664), UINT32_C(0x800fe8a8), 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d668), 4, R(17), 0x1cu, 4, 0));
    TRY(require_full(run, UINT32_C(0x8005d670), R(4), &stats));
    TRY(load_abs(run, UINT32_C(0x8005d670), 3, stats + 0x14u, 2, 0));
    R(2) = unsigned_lt_word(R(3), 999u); motion = R(2);
    R(2) = add_constant(R(3), 1u);
    TRY(decide_equal(run, UINT32_C(0x8005d67c), motion, zero, &branch));
    if (!branch) {
        TRY(store_abs(run, UINT32_C(0x8005d684), stats + 0x14u, 2, R(2)));
        TRY(load_at(run, UINT32_C(0x8005d688), 5, R(17), 0, 2, 1));
        TRY(stat_call(run, UINT32_C(0x8005d68c), 24u, R(5)));
    }
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d698), 2, UINT32_C(0x800fdb84), 2, 1));
    TRY(decide_equal(run, UINT32_C(0x8005d6a0), R(2), zero, &branch));
    if (branch) {
        TRY(load_at(run, UINT32_C(0x8005d6a8), 2, R(17), 4u, 2, 1));
        {
            Nba97GameBallAcquireWord index = R(2);
            TRY(negative_with_sll2_delay(run, UINT32_C(0x8005d6b0),
                index, &negative));
            if (!negative) {
                R(2) = add_word(R(18), R(2));
                TRY(increment_controller(run, UINT32_C(0x8005d6bc),
                    UINT32_C(0x8005d6c4), UINT32_C(0x8005d6d0), R(2),
                    0xbau, 0x14u));
            }
        }
    }
    goto other_selected;

old_team_equal:
    TRY(load_at(run, UINT32_C(0x8005d7bc), 2, R(16), 0x52u, 2, 1));
    R(2) = shift_left2(R(2));
    R(2) = add_word(R(18), R(2));
    TRY(load_at(run, UINT32_C(0x8005d7cc), 4, R(2), 0xdau, 4, 0));
    TRY(require_full(run, UINT32_C(0x8005d7d4), R(4), &stats));
    TRY(load_abs(run, UINT32_C(0x8005d7d4), 3, stats + 0x14u, 2, 0));
    R(2) = unsigned_lt_word(R(3), 999u); motion = R(2);
    R(2) = add_constant(R(3), 1u);
    TRY(decide_equal(run, UINT32_C(0x8005d7e0), motion, zero, &branch));
    if (!branch) {
        TRY(store_abs(run, UINT32_C(0x8005d7e8), stats + 0x14u, 2, R(2)));
        TRY(load_at(run, UINT32_C(0x8005d7ec), 5, R(16), 0x52u, 2, 1));
        TRY(stat_call(run, UINT32_C(0x8005d7f0), 24u, R(5)));
    }
    TRY(load_at(run, UINT32_C(0x8005d7f8), 2, R(16), 0x52u, 2, 1));
    R(2) = shift_left2(R(2));
    R(1) = add_constant(R(2), UINT32_C(0x80020000));
    TRY(load_at(run, UINT32_C(0x8005d80c), 2, R(1), 0xbecu, 4, 0));
    known(&R(3), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d814), 3, UINT32_C(0x800fdb84), 2, 1));
    TRY(load_at(run, UINT32_C(0x8005d818), 2, R(2), 4u, 2, 1));
    TRY(decide_equal(run, UINT32_C(0x8005d81c), R(3), zero, &branch));
    if (!branch) goto other_selected;
    {
        Nba97GameBallAcquireWord index = R(2);
        TRY(negative_with_sll2_delay(run, UINT32_C(0x8005d6b0),
            index, &negative));
        if (!negative) {
            R(2) = add_word(R(18), R(2));
            TRY(increment_controller(run, UINT32_C(0x8005d6bc),
                UINT32_C(0x8005d6c4), UINT32_C(0x8005d6d0), R(2),
                0xbau, 0x14u));
        }
    }

other_selected:
    TRY(load_at(run, UINT32_C(0x8005d6d4), 2, R(16), 4u, 4, 0));
    TRY(load_at(run, UINT32_C(0x8005d6dc), 2, R(2), 0x52u, 2, 1));
    {
        Nba97GameBallAcquireWord index = R(2);
        TRY(negative_with_sll2_delay(run, UINT32_C(0x8005d6e4),
            index, &negative));
        if (negative) goto common;
    }
    known(&R(3), UINT32_C(0x80020bec));
    R(5) = add_word(R(2), R(3));
    TRY(load_at(run, UINT32_C(0x8005d6f8), 2, R(5), 0, 4, 0));
    TRY(load_at(run, UINT32_C(0x8005d700), 4, R(2), 0x1cu, 4, 0));
    TRY(require_full(run, UINT32_C(0x8005d708), R(4), &stats));
    TRY(load_abs(run, UINT32_C(0x8005d708), 3, stats + 0x18u, 2, 0));
    R(2) = unsigned_lt_word(R(3), 999u); motion = R(2);
    R(2) = add_constant(R(3), 1u);
    TRY(decide_equal(run, UINT32_C(0x8005d714), motion, zero, &branch));
    if (branch) goto common;
    TRY(store_abs(run, UINT32_C(0x8005d71c), stats + 0x18u, 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d720), 2, R(5), 0, 4, 0));
    TRY(load_at(run, UINT32_C(0x8005d728), 2, R(2), 4u, 2, 1));
    {
        Nba97GameBallAcquireWord index = R(2);
        TRY(negative_with_sll2_delay(run, UINT32_C(0x8005d730),
            index, &negative));
        if (!negative) {
            R(1) = add_constant(R(2), UINT32_C(0x80100000));
            TRY(increment_controller(run, UINT32_C(0x8005d740),
                UINT32_C(0x8005d748), UINT32_C(0x8005d754), R(1),
                UINT32_C(0xffffdc50), 0x18u));
        }
    }
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d75c), 2, UINT32_C(0x800fdbb0), 2, 0));
    TRY(decide_equal(run, UINT32_C(0x8005d764), R(2), zero, &branch));
    if (!branch) {
        TRY(load_at(run, UINT32_C(0x8005d76c), 2, R(5), 0, 4, 0));
        TRY(load_at(run, UINT32_C(0x8005d774), 5, R(2), 0, 2, 1));
        TRY(stat_call(run, UINT32_C(0x8005d778), 12u, R(5)));
    }
    goto common;

same_team:
    run->out->same_team_claim = 1;
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d830), 2, UINT32_C(0x800fdbd8), 2, 1));
    known(&R(1), UINT32_C(0x80100000));
    TRY(store_abs(run, UINT32_C(0x8005d838), UINT32_C(0x800fdb96), 2, R(4)));
    TRY(decide_equal(run, UINT32_C(0x8005d83c), R(2), zero, &branch)); if (branch) goto common;
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d848), 2, UINT32_C(0x800fe8cc), 2, 1));
    TRY(decide_equal(run, UINT32_C(0x8005d850), R(2), zero, &branch)); if (!branch) goto common;
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d85c), 2, UINT32_C(0x800fdbb0), 2, 0));
    TRY(decide_equal(run, UINT32_C(0x8005d864), R(2), zero, &branch)); if (!branch) goto common;
    known(&R(2), UINT32_C(0x80100000));
    TRY(load_abs(run, UINT32_C(0x8005d870), 2, UINT32_C(0x800fdb90), 2, 1));
    R(2) = signed_lt_word(R(2), 128);
    TRY(decide_equal(run, UINT32_C(0x8005d87c), R(2), zero, &branch));
    if (branch) goto common;
    TRY(load_at(run, UINT32_C(0x8005d884), 2, R(17), 0, 4, 0));
    R(2) = signed_lt_word(R(2), 5);
    TRY(decide_equal(run, UINT32_C(0x8005d890), R(2), zero, &branch));
    known(&R(4), branch ? 10000u : 20000u);
    TRY(invoke(run, branch ? UINT32_C(0x8005d898) : UINT32_C(0x8005d8a8),
        UINT32_C(0x800295c8), NBA97_GAME_BALL_ACQUIRE_CHILD_800295C8, 1));
    known(&R(4), branch ? 3u : 4u);
    TRY(invoke(run, UINT32_C(0x8005d8b4), UINT32_C(0x80029590),
        NBA97_GAME_BALL_ACQUIRE_CHILD_80029590, 1));
    known(&R(18), UINT32_C(0x800fe86e)); known(&R(2), 3u);
    TRY(store_abs(run, UINT32_C(0x8005d8c8), UINT32_C(0x800fe86e), 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d8cc), 2, R(17), 8u, 4, 0));
    TRY(load_at(run, UINT32_C(0x8005d8d0), 3, R(16), 0x10u, 4, 0));
    R(2).word ^= R(3).word; R(2).known_mask &= R(3).known_mask;
    TRY(decide_negative(run, UINT32_C(0x8005d8dc), R(2), &negative)); if (negative) goto common;
    TRY(load_at(run, UINT32_C(0x8005d8e4), 2, R(17), 0xdeu, 1, 0));
    R(2) = add_constant(R(2), 1u);
    TRY(store_at(run, UINT32_C(0x8005d8f0), R(17), 0xdeu, 1, R(2)));
    known(&R(2), UINT32_MAX);
    TRY(store_at(run, UINT32_C(0x8005d8f8), R(16), 0x58u, 2, R(2)));
    TRY(store_at(run, UINT32_C(0x8005d8fc), R(16), 0x56u, 2, R(2)));
    TRY(load_at(run, UINT32_C(0x8005d900), 4, R(17), 0x1cu, 4, 0));
    TRY(require_full(run, UINT32_C(0x8005d908), R(4), &stats));
    TRY(load_abs(run, UINT32_C(0x8005d908), 3, stats + 0x0eu, 2, 0));
    R(2) = unsigned_lt_word(R(3), 999u); motion = R(2);
    R(2) = add_constant(R(3), 1u);
    TRY(decide_equal(run, UINT32_C(0x8005d914), motion, zero, &branch));
    if (!branch) {
        TRY(store_abs(run, UINT32_C(0x8005d91c), stats + 0x0eu, 2, R(2)));
        TRY(load_at(run, UINT32_C(0x8005d920), 5, R(17), 0, 2, 1));
        TRY(stat_call(run, UINT32_C(0x8005d924), 13u, R(5)));
    }
    TRY(load_at(run, UINT32_C(0x8005d92c), 2, R(17), 0x46u, 2, 0));
    R(2) = add_constant(R(2), UINT32_C(0xffffffd4));
    R(2) = unsigned_lt_word(R(2), 18u);
    TRY(decide_equal(run, UINT32_C(0x8005d93c), R(2), zero, &branch));
    if (!branch) goto common;
    TRY(load_at(run, UINT32_C(0x8005d944), 2, R(17), 4u, 2, 1));
    R(4) = R(17); /* BLTZ delay */
    TRY(decide_negative(run, UINT32_C(0x8005d94c), R(2), &negative));
    if (negative) goto negative_controller_velocity;
    TRY(load_at(run, UINT32_C(0x8005d954), 2, R(17), 0x14u, 2, 0));
    TRY(load_at(run, UINT32_C(0x8005d958), 3, R(17), 0x16u, 2, 0));
    R(2) = arithmetic_half16(R(2)); R(3) = arithmetic_half16(R(3));
    TRY(store_at(run, UINT32_C(0x8005d96c), R(17), 0x14u, 2, R(2)));
    known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8005d978));
    TRY(store_at(run, UINT32_C(0x8005d974), R(17), 0x16u, 2, R(3)));
    TRY(invoke(run, UINT32_C(0x8005d970), UINT32_C(0x8005ce4c),
        NBA97_GAME_BALL_ACQUIRE_CHILD_8005CE4C, 1));
    TRY(load_at(run, UINT32_C(0x8005d978), 2, R(17), 4u, 2, 1));
    R(2) = shift_left2(R(2)); R(2) = add_word(R(18), R(2));
    TRY(increment_controller(run, UINT32_C(0x8005d988), UINT32_C(0x8005d990),
        UINT32_C(0x8005d9a0), R(2), UINT32_C(0xfffff3e2), 0x0eu));
    goto common;
negative_controller_velocity:
    TRY(load_at(run, UINT32_C(0x8005d9a4), 2, R(17), 0x14u, 2, 0));
    TRY(load_at(run, UINT32_C(0x8005d9a8), 3, R(17), 0x16u, 2, 0));
    R(2) = arithmetic_half16(R(2)); R(3) = arithmetic_half16(R(3));
    TRY(store_at(run, UINT32_C(0x8005d9bc), R(4), 0x14u, 2, R(2)));
    known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8005d9c8));
    TRY(store_at(run, UINT32_C(0x8005d9c4), R(4), 0x16u, 2, R(3)));
    TRY(invoke(run, UINT32_C(0x8005d9c0), UINT32_C(0x8005ce4c),
        NBA97_GAME_BALL_ACQUIRE_CHILD_8005CE4C, 1));

common:
    TRY(cleanup(run));
    return finish_frame(run);
}

int nba97_game_ball_acquire(Nba97GameBallAcquireContext* context,
    Nba97GameBallAcquireProgress* out) {
    Run run;
    int status = begin(context, out, &run);
    if (status != NBA97_TEXT_COMPLETE) return status;
    return execute(&run);
}
