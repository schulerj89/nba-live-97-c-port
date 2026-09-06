#include "game_stream_queue_count.h"

#include <string.h>

#define QUEUE_HEAD UINT32_C(0x800c43a0)
#define LOCK_COUNTER UINT32_C(0x800c4410)

typedef struct Run {
    Nba97GameStreamQueueCountContext* context;
    Nba97GameStreamQueueCountProgress* out;
    Nba97GameStreamQueueCountMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Run* run) {
    run->out->machine = run->machine;
    run->out->returned_count = R(NBA97_MATCH_INITIALIZE_V0);
}

static void stop(Run* run, uint32_t pc, uint32_t address,
    uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameStreamQueueCountWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameStreamQueueCountMachine* machine) {
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

static int validate(Nba97GameStreamQueueCountContext* context,
    Nba97GameStreamQueueCountProgress* out, Run* run) {
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

/* Byte-domain carry enumeration retains precisely the bytes invariant across
 * every concrete wrapping ADDIU represented by a partial knownness mask. */
static Nba97GameStreamQueueCountWord add_words(
    Nba97GameStreamQueueCountWord left,
    Nba97GameStreamQueueCountWord right) {
    Nba97GameStreamQueueCountWord result;
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

static Nba97GameStreamQueueCountWord add_constant(
    Nba97GameStreamQueueCountWord source, uint32_t constant) {
    Nba97GameStreamQueueCountWord value;
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
    const Nba97GameStreamQueueCountWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameStreamQueueCountAccess* event =
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
    uint32_t pc, Nba97GameStreamQueueCountWord* value) {
    Nba97GameStreamQueueCountWord loaded = {0, 0};
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
    uint32_t pc, const Nba97GameStreamQueueCountWord* value) {
    Nba97GameStreamQueueCountWord stored = *value;
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

static int register_address(Run* run,
    Nba97GameStreamQueueCountWord base, uint32_t offset, uint32_t pc,
    uint32_t* address) {
    Nba97GameStreamQueueCountWord value = add_constant(base, offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static int decide_zero(Run* run,
    const Nba97GameStreamQueueCountWord* value, uint32_t pc,
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
    const Nba97GameStreamQueueCountWord* left,
    const Nba97GameStreamQueueCountWord* right, uint32_t pc,
    int* equal) {
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

static int invoke_child(Run* run, uint32_t pc, uint32_t entry,
    uint8_t kind) {
    Nba97GameStreamQueueCountEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[kind] + 1u;
    event.kind = kind;
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
    ++run->out->call_count[kind];
    return NBA97_TEXT_COMPLETE;
}

static int stack_access_address(Run* run, uint32_t offset, uint32_t pc,
    uint32_t* address) {
    return register_address(run, R(NBA97_MATCH_INITIALIZE_FP), offset, pc,
        address);
}

static int restore(Run* run, uint32_t pc, uint32_t offset, unsigned reg,
    Nba97GameStreamQueueCountWord* reported) {
    uint32_t address;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), offset, pc,
        &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_stream_queue_count(Nba97GameStreamQueueCountContext* context,
    Nba97GameStreamQueueCountProgress* out) {
    Run storage;
    Run* run = &storage;
    Nba97GameStreamQueueCountWord value;
    Nba97GameStreamQueueCountWord constant;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* 0x80084448..0x80084458: establish the frame and clear its local count
     * before any global read or callback can alias those writes. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x1cu,
        UINT32_C(0x8008444c), &address));
    out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(write_value(run, address, 4, UINT32_C(0x8008444c),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x18u,
        UINT32_C(0x80084450), &address));
    out->saved_s8 = R(NBA97_MATCH_INITIALIZE_FP);
    TRY(write_value(run, address, 4, UINT32_C(0x80084450),
        &R(NBA97_MATCH_INITIALIZE_FP)));
    R(NBA97_MATCH_INITIALIZE_FP) = R(NBA97_MATCH_INITIALIZE_SP);
    TRY(stack_access_address(run, 0x14u, UINT32_C(0x80084458), &address));
    set_known(&value, 0);
    TRY(write_value(run, address, 4, UINT32_C(0x80084458), &value));

    /* 0x8008445C..0x80084478: an initially null head returns -1 without
     * touching the counter or either typed child. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_value(run, QUEUE_HEAD, 4, UINT32_C(0x80084460),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->initial_head = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80084468), &branch));
    if (branch) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
        goto restore_registers;
    }

    /* 0x8008447C..0x800844AC: lock first, increment the callback-live
     * counter, then reread the head and publish it through callback-live s8. */
    TRY(invoke_child(run, UINT32_C(0x8008447c),
        UINT32_C(0x80093d94),
        NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800c0000));
    TRY(read_value(run, LOCK_COUNTER, 4, UINT32_C(0x80084488),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), 1);
    R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    TRY(write_value(run, LOCK_COUNTER, 4, UINT32_C(0x8008449c),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    out->counter_after_increment = R(NBA97_MATCH_INITIALIZE_V1);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_value(run, QUEUE_HEAD, 4, UINT32_C(0x800844a4),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(stack_access_address(run, 0x10u, UINT32_C(0x800844ac), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x800844ac),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->active_pointer = R(NBA97_MATCH_INITIALIZE_V0);

loop:
    /* 0x800844B0..0x800844F4: reload the local for each sentinel comparison,
     * reload it again for dereference, and terminate on a null link. */
    ++run->out->loop_iterations;
    TRY(stack_access_address(run, 0x10u, UINT32_C(0x800844b0), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x800844b0),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffffe));
    constant = R(NBA97_MATCH_INITIALIZE_V1);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V0), &constant,
        UINT32_C(0x800844b8), &branch));
    if (branch)
        goto loop_exit;
    TRY(stack_access_address(run, 0x10u, UINT32_C(0x800844c0), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x800844c0),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_MAX);
    constant = R(NBA97_MATCH_INITIALIZE_V1);
    TRY(decide_equal(run, &R(NBA97_MATCH_INITIALIZE_V0), &constant,
        UINT32_C(0x800844c8), &branch));
    if (branch)
        goto loop_exit;
    TRY(stack_access_address(run, 0x10u, UINT32_C(0x800844d0), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x800844d0),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_V0), 0,
        UINT32_C(0x800844d8), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x800844d8),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x800844e0), &branch));
    if (branch)
        goto loop_exit;

    /* 0x800844F8..0x8008453C: count one nonzero link, then perform both
     * evidenced node rereads before updating the local pointer. */
    TRY(stack_access_address(run, 0x14u, UINT32_C(0x800844f8), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x800844f8),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), 1);
    R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(stack_access_address(run, 0x14u, UINT32_C(0x80084508), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80084508),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    ++run->out->links_counted;
    TRY(stack_access_address(run, 0x10u, UINT32_C(0x8008450c), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x8008450c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_V0), 0,
        UINT32_C(0x80084514), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x80084514),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x8008451c), &branch));
    if (!branch) {
        TRY(stack_access_address(run, 0x10u,
            UINT32_C(0x80084524), &address));
        TRY(read_value(run, address, 4, UINT32_C(0x80084524),
            &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_V0), 0,
            UINT32_C(0x8008452c), &address));
        TRY(read_value(run, address, 4, UINT32_C(0x8008452c),
            &R(NBA97_MATCH_INITIALIZE_V1)));
        TRY(stack_access_address(run, 0x10u,
            UINT32_C(0x80084534), &address));
        TRY(write_value(run, address, 4, UINT32_C(0x80084534),
            &R(NBA97_MATCH_INITIALIZE_V1)));
        out->active_pointer = R(NBA97_MATCH_INITIALIZE_V1);
    }
    goto loop;

loop_exit:
    /* 0x80084540..0x80084560: decrement the live counter before unlock;
     * refusal or a bounded cycle never fabricates this cleanup sequence. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800c0000));
    TRY(read_value(run, LOCK_COUNTER, 4, UINT32_C(0x80084544),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), UINT32_MAX);
    R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    TRY(write_value(run, LOCK_COUNTER, 4, UINT32_C(0x80084558),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    out->counter_after_decrement = R(NBA97_MATCH_INITIALIZE_V1);
    TRY(invoke_child(run, UINT32_C(0x8008455c),
        UINT32_C(0x80093dd4),
        NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093DD4));

    /* 0x80084564..0x8008456C: unlock may redirect s8 or mutate the count;
     * load the raw result only after that callback returns. */
    TRY(stack_access_address(run, 0x14u, UINT32_C(0x80084564), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x80084564),
        &R(NBA97_MATCH_INITIALIZE_V0)));

restore_registers:
    /* 0x80084570..0x80084584: select live s8 as sp, restore ra/s8 in source
     * order, advance sp, and consume restored ra after JR's NOP delay. */
    R(NBA97_MATCH_INITIALIZE_SP) = R(NBA97_MATCH_INITIALIZE_FP);
    TRY(restore(run, UINT32_C(0x80084574), 0x1cu,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x80084578), 0x18u,
        NBA97_MATCH_INITIALIZE_FP, &out->restored_s8));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80084580), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
