#include "game_stream_readiness.h"

#include <limits.h>
#include <string.h>

#define STREAM_FLAG UINT32_C(0x800f0fdc)

typedef struct Run {
    Nba97GameStreamReadinessContext* context;
    Nba97GameStreamReadinessProgress* out;
    Nba97GameStreamReadinessMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Run* run) {
    run->out->machine = run->machine;
}

static void stop(Run* run, uint32_t pc, uint32_t address,
    uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameStreamReadinessWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameStreamReadinessMachine* machine) {
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

static int validate(Nba97GameStreamReadinessContext* context,
    Nba97GameStreamReadinessProgress* out, Run* run) {
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
        uint64_t region_size = (uint64_t)a->size;
        if (!a->data || !a->size ||
            region_size > UINT64_C(0x100000000) ||
            (uint64_t)a->base + region_size > UINT64_C(0x100000000))
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

/* Enumerating each possible byte and carry preserves exactly the known bytes
 * of wrapping ADDIU addresses, including partially-known sp/s8 inputs. */
static Nba97GameStreamReadinessWord add_words(
    Nba97GameStreamReadinessWord left,
    Nba97GameStreamReadinessWord right) {
    Nba97GameStreamReadinessWord result;
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

static Nba97GameStreamReadinessWord add_constant(
    Nba97GameStreamReadinessWord source, uint32_t constant) {
    Nba97GameStreamReadinessWord value;
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
    const Nba97GameStreamReadinessWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameStreamReadinessAccess* event =
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
    uint32_t pc, Nba97GameStreamReadinessWord* value) {
    Nba97GameStreamReadinessWord loaded = {0, 0};
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
    uint32_t pc, const Nba97GameStreamReadinessWord* value) {
    Nba97GameStreamReadinessWord stored = *value;
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

static int register_address(Run* run, Nba97GameStreamReadinessWord base,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GameStreamReadinessWord value = add_constant(base, offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameStreamReadinessWord load_lh(
    Nba97GameStreamReadinessWord raw) {
    Nba97GameStreamReadinessWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static int64_t signed_word(uint32_t value) {
    return value < UINT32_C(0x80000000) ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GameStreamReadinessWord* value,
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

static Nba97GameStreamReadinessWord signed_less_two(
    const Nba97GameStreamReadinessWord* value) {
    Nba97GameStreamReadinessWord result;
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    result.word = signed_word(value->word) < 2;
    result.known_mask = 0x0eu;
    if (maximum < 2)
        set_known(&result, 1);
    else if (minimum >= 2)
        set_known(&result, 0);
    return result;
}

static int decide_zero(Run* run,
    const Nba97GameStreamReadinessWord* value, uint32_t pc, int* is_zero) {
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

static int invoke_child(Run* run) {
    Nba97GameStreamReadinessEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80088d38));
    stop(run, UINT32_C(0x80088d30), 0, UINT32_C(0x80084448));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = UINT32_C(0x80088d30);
    event.delay_slot_pc = UINT32_C(0x80088d34);
    event.entry = UINT32_C(0x80084448);
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[
        NBA97_GAME_STREAM_READINESS_CHILD_80084448] + 1u;
    event.kind = NBA97_GAME_STREAM_READINESS_CHILD_80084448;
    event.argument_count = 0;
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
    ++run->out->call_count[NBA97_GAME_STREAM_READINESS_CHILD_80084448];
    return NBA97_TEXT_COMPLETE;
}

static int restore(Run* run, uint32_t pc, uint32_t offset, unsigned reg,
    Nba97GameStreamReadinessWord* reported) {
    uint32_t address;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), offset, pc,
        &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_stream_readiness(Nba97GameStreamReadinessContext* context,
    Nba97GameStreamReadinessProgress* out) {
    Run storage;
    Run* run = &storage;
    Nba97GameStreamReadinessWord raw;
    Nba97GameStreamReadinessWord branch_value;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* 0x80088D0C..0x80088D18: create the 0x18-byte frame, spill ra/s8,
     * and make s8 the epilogue's independently mutable frame selector. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u,
        UINT32_C(0x80088d10), &address));
    out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(write_value(run, address, 4, UINT32_C(0x80088d10),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
        UINT32_C(0x80088d14), &address));
    out->saved_s8 = R(NBA97_MATCH_INITIALIZE_FP);
    TRY(write_value(run, address, 4, UINT32_C(0x80088d14),
        &R(NBA97_MATCH_INITIALIZE_FP)));
    R(NBA97_MATCH_INITIALIZE_FP) = R(NBA97_MATCH_INITIALIZE_SP);

    /* 0x80088D1C..0x80088D2C: signed LH is followed by its load NOP and
     * branch NOP. A definitely-zero flag skips the sole child. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800f0000));
    TRY(read_value(run, STREAM_FLAG, 2, UINT32_C(0x80088d20), &raw));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
    out->loaded_flag = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80088d28), &branch));
    if (branch)
        goto return_zero;

    /* 0x80088D30..0x80088D4C: call readiness service with a NOP delay,
     * then retain SLTI's known upper zero bytes before deciding its BEQ. */
    TRY(invoke_child(run));
    R(NBA97_MATCH_INITIALIZE_V1) = signed_less_two(
        &R(NBA97_MATCH_INITIALIZE_V0));
    branch_value = R(NBA97_MATCH_INITIALIZE_V1);
    TRY(decide_zero(run, &branch_value, UINT32_C(0x80088d3c), &branch));
    if (branch)
        goto return_zero;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    goto restore_registers;

return_zero:
    /* 0x80088D58..0x80088D60: common false result. The full source span
     * also contains unreachable duplicate J/NOP words at 0x80088D50/54. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0);

restore_registers:
    /* 0x80088D64..0x80088D78: select live s8 as sp, reload ra then s8,
     * advance live sp, and consume the restored ra after JR's NOP delay. */
    R(NBA97_MATCH_INITIALIZE_SP) = R(NBA97_MATCH_INITIALIZE_FP);
    TRY(restore(run, UINT32_C(0x80088d68), 0x14u,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x80088d6c), 0x10u,
        NBA97_MATCH_INITIALIZE_FP, &out->restored_s8));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80088d74), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
