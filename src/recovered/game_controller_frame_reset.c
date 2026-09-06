#include "game_controller_frame_reset.h"

#include <string.h>

#define TIMER_ADDRESS UINT32_C(0x800fe90e)
#define DELTA_ADDRESS UINT32_C(0x800fdb6c)
#define POINTER_TABLE_ADDRESS UINT32_C(0x800fdc50)

typedef struct Nba97GameControllerFrameResetRun {
    Nba97GameControllerFrameResetContext* context;
    Nba97GameControllerFrameResetProgress* out;
    Nba97GameControllerFrameResetRegisters registers;
} Nba97GameControllerFrameResetRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameControllerFrameResetRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameControllerFrameResetRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameControllerFrameResetRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
    return width == 4 ? UINT32_MAX :
        (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}

static void journal(Nba97GameControllerFrameResetRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameControllerFrameResetWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameControllerFrameResetAccess* event =
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

static int locate(Nba97GameControllerFrameResetRun* run, uint32_t address,
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

static int read_value(Nba97GameControllerFrameResetRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    Nba97GameControllerFrameResetWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameControllerFrameResetWord loaded;
    unsigned i;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, address, width, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (uint8_t)(1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_CONTROLLER_FRAME_RESET_READ, pc, address,
        width, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameControllerFrameResetRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    const Nba97GameControllerFrameResetWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameControllerFrameResetWord stored = *value;
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
    journal(run, NBA97_GAME_CONTROLLER_FRAME_RESET_STORE, pc, address,
        width, &stored);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(
    const Nba97GameControllerFrameResetRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameControllerFrameResetContext* context,
    Nba97GameControllerFrameResetProgress* out,
    Nba97GameControllerFrameResetRun* run) {
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

/* Propagate byte knowledge through wrapping ADDIU without inventing carries
 * from unknown source bytes. */
static Nba97GameControllerFrameResetWord add_constant(
    Nba97GameControllerFrameResetWord input, uint32_t constant) {
    Nba97GameControllerFrameResetWord result;
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

/* SUBU uses a byte borrow chain. Unknown bytes range over all 256 values, so
 * an output byte is invariant only when both operands and its input borrow are
 * fixed; the next borrow can still become known at an extreme. */
static Nba97GameControllerFrameResetWord subtract_words(
    Nba97GameControllerFrameResetWord left,
    Nba97GameControllerFrameResetWord right) {
    Nba97GameControllerFrameResetWord result;
    unsigned byte;
    unsigned borrow_mask = 1u;
    result.word = left.word - right.word;
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        unsigned next_borrow_mask = 0;
        unsigned first_output = 0;
        int first = 1;
        int invariant = 1;
        unsigned borrow;
        unsigned left_known = (left.known_mask >> byte) & 1u;
        unsigned right_known = (right.known_mask >> byte) & 1u;
        unsigned left_min = left_known ?
            ((left.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned left_max = left_known ? left_min : 255u;
        unsigned right_min = right_known ?
            ((right.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned right_max = right_known ? right_min : 255u;
        for (borrow = 0; borrow <= 1; ++borrow) {
            unsigned output;
            if (!(borrow_mask & (1u << borrow)))
                continue;
            if (left_max >= right_min + borrow)
                next_borrow_mask |= 1u;
            if (left_min < right_max + borrow)
                next_borrow_mask |= 2u;
            if (!left_known || !right_known) {
                invariant = 0;
                continue;
            }
            output = (left_min - right_min - borrow) & 0xffu;
            if (first) {
                first_output = output;
                first = 0;
            } else if (output != first_output) {
                invariant = 0;
            }
        }
        if (invariant && !first)
            result.known_mask =
                (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
        borrow_mask = next_borrow_mask;
    }
    return result;
}

static Nba97GameControllerFrameResetWord sign_extend_halfword(
    Nba97GameControllerFrameResetWord value) {
    Nba97GameControllerFrameResetWord result;
    result.word = (value.word & UINT32_C(0x8000)) ?
        (value.word & UINT32_C(0xffff)) | UINT32_C(0xffff0000) :
        value.word & UINT32_C(0xffff);
    result.known_mask = (uint8_t)(value.known_mask & 3u);
    if (value.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GameControllerFrameResetWord zero_extend_halfword(
    Nba97GameControllerFrameResetWord value) {
    value.word &= UINT32_C(0xffff);
    value.known_mask = (uint8_t)((value.known_mask & 3u) | 0x0cu);
    return value;
}

static Nba97GameControllerFrameResetWord shift_left_16(
    Nba97GameControllerFrameResetWord value) {
    Nba97GameControllerFrameResetWord result;
    result.word = value.word << 16;
    result.known_mask =
        (uint8_t)(((value.known_mask & 3u) << 2u) | 3u);
    return result;
}

static int effective_address(Nba97GameControllerFrameResetRun* run,
    unsigned base_register, uint32_t offset, uint32_t pc,
    uint32_t* address) {
    if (REG(run, base_register).known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = REG(run, base_register).word + offset;
    return NBA97_TEXT_COMPLETE;
}

static int decide_zero(Nba97GameControllerFrameResetRun* run,
    const Nba97GameControllerFrameResetWord* value, uint32_t pc,
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
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int decide_nonnegative(Nba97GameControllerFrameResetRun* run,
    const Nba97GameControllerFrameResetWord* value, uint32_t pc,
    int* nonnegative) {
    if (!(value->known_mask & 8u)) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *nonnegative = (value->word & UINT32_C(0x80000000)) == 0;
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameControllerFrameResetRun* run) {
    Nba97GameControllerFrameResetEvent event;
    int accepted;
    REG(run, NBA97_MATCH_INITIALIZE_RA) =
        (Nba97GameControllerFrameResetWord){UINT32_C(0x80067654), 0x0f};
    stop(run, UINT32_C(0x8006764c), 0, UINT32_C(0x80083eec));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = UINT32_C(0x8006764c);
    event.delay_slot_pc = UINT32_C(0x80067650);
    event.entry = UINT32_C(0x80083eec);
    event.operation = run->out->operations;
    event.kind = NBA97_GAME_CONTROLLER_FRAME_RESET_83EEC;
    event.argument_count = 0;
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
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_controller_frame_reset(
    Nba97GameControllerFrameResetContext* context,
    Nba97GameControllerFrameResetProgress* out) {
    Nba97GameControllerFrameResetRun storage;
    Nba97GameControllerFrameResetRun* run = &storage;
    Nba97GameControllerFrameResetWord value;
    Nba97GameControllerFrameResetWord zero = {0, 0x0f};
    uint32_t address;
    int is_zero;
    int nonnegative;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800675E4..0x800675F0: allocate the 0x20-byte frame, form the
     * timer address in a0, and save ra at the wrapped live sp+0x18 address. */
    REG(run, NBA97_MATCH_INITIALIZE_SP) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = REG(run, NBA97_MATCH_INITIALIZE_SP).word;
    REG(run, NBA97_MATCH_INITIALIZE_A0) =
        (Nba97GameControllerFrameResetWord){UINT32_C(0x80100000), 0x0f};
    REG(run, NBA97_MATCH_INITIALIZE_A0) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffe90e));
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x18u,
        UINT32_C(0x800675f0), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x800675f0),
        &REG(run, NBA97_MATCH_INITIALIZE_RA)));

    /* 0x800675F4 LH sign-extends the timer; the 0x800675FC BEQ always executes
     * MOVE v1,v0 at 0x80067600 before either branch or an unknown decision. */
    TRY(read_value(run, TIMER_ADDRESS, 2, UINT32_C(0x800675f4), &value));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = sign_extend_halfword(value);
    out->initial_timer = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_V1) =
        REG(run, NBA97_MATCH_INITIALIZE_V0); /* 0x80067600 delay */
    TRY(decide_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x800675fc), &is_zero));
    if (!is_zero) {
        /* GAMEONLY 0x80067604..0x80067620: LHU zero-extends the delta, SUBU
         * wraps, SH publishes the low half before SLL/BGEZ sign-test it. */
        REG(run, NBA97_MATCH_INITIALIZE_V0) =
            (Nba97GameControllerFrameResetWord){UINT32_C(0x80100000), 0x0f};
        TRY(read_value(run, DELTA_ADDRESS, 2, UINT32_C(0x80067608),
            &value));
        REG(run, NBA97_MATCH_INITIALIZE_V0) = zero_extend_halfword(value);
        out->delta = REG(run, NBA97_MATCH_INITIALIZE_V0);
        REG(run, NBA97_MATCH_INITIALIZE_V0) = subtract_words(
            REG(run, NBA97_MATCH_INITIALIZE_V1),
            REG(run, NBA97_MATCH_INITIALIZE_V0));
        out->adjusted_timer = REG(run, NBA97_MATCH_INITIALIZE_V0);
        TRY(write_value(run, TIMER_ADDRESS, 2, UINT32_C(0x80067614),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        out->timer_updated = 1;
        REG(run, NBA97_MATCH_INITIALIZE_V0) =
            shift_left_16(REG(run, NBA97_MATCH_INITIALIZE_V0));
        TRY(decide_nonnegative(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x8006761c), &nonnegative));
        if (!nonnegative) {
            /* 0x80067624 runs only when the stored low half is negative. */
            TRY(write_value(run, TIMER_ADDRESS, 2,
                UINT32_C(0x80067624), &zero));
            out->timer_clamped = 1;
        }
    }

    /* GAMEONLY 0x80067628..0x80067648: each iteration reloads a live table
     * word, increments a0 before SH, then advances v1 in the BNE delay slot.
     * The final not-taken branch still leaves v1 at table+0x20. */
    REG(run, NBA97_MATCH_INITIALIZE_A0) =
        (Nba97GameControllerFrameResetWord){0, 0x0f};
    REG(run, NBA97_MATCH_INITIALIZE_V1) =
        (Nba97GameControllerFrameResetWord){POINTER_TABLE_ADDRESS, 0x0f};
    for (;;) {
        TRY(effective_address(run, NBA97_MATCH_INITIALIZE_V1, 0,
            UINT32_C(0x80067634), &address));
        TRY(read_value(run, address, 4, UINT32_C(0x80067634),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        REG(run, NBA97_MATCH_INITIALIZE_A0) = add_constant(
            REG(run, NBA97_MATCH_INITIALIZE_A0), 1);
        TRY(effective_address(run, NBA97_MATCH_INITIALIZE_V0, 0x28u,
            UINT32_C(0x8006763c), &address));
        TRY(write_value(run, address, 2, UINT32_C(0x8006763c), &zero));
        ++out->controller_slots_cleared;
        REG(run, NBA97_MATCH_INITIALIZE_V0) =
            (Nba97GameControllerFrameResetWord){
                (uint32_t)((int32_t)REG(run,
                    NBA97_MATCH_INITIALIZE_A0).word < 8), 0x0f};
        REG(run, NBA97_MATCH_INITIALIZE_V1) = add_constant(
            REG(run, NBA97_MATCH_INITIALIZE_V1), 4); /* 0x80067648 */
        if (REG(run, NBA97_MATCH_INITIALIZE_V0).word == 0)
            break;
    }

    /* 0x8006764C JAL writes ra=0x80067654 before its NOP delay and invokes
     * the sole typed child with the full live register file. */
    TRY(invoke(run));

    /* GAMEONLY 0x80067654..0x80067660: reload ra through callback-mutable sp,
     * execute sp+=0x20, then consume the possibly unknown ra at JR. */
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x18u,
        UINT32_C(0x80067654), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x80067654),
        &REG(run, NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = REG(run, NBA97_MATCH_INITIALIZE_RA);
    REG(run, NBA97_MATCH_INITIALIZE_SP) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_SP), UINT32_C(0x20));
    if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8006765c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    /* JR at 0x8006765C executes the NOP delay slot at 0x80067660. */
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
