#include "game_clear_ordering_table.h"

#include <string.h>

#define DEBUG_LEVEL_ADDRESS UINT32_C(0x800c55c2)
#define DEBUG_TARGET_ADDRESS UINT32_C(0x800c55bc)
#define DISPATCH_TABLE_ADDRESS UINT32_C(0x800c55b8)
#define DEBUG_FORMAT_ADDRESS UINT32_C(0x80028340)
#define ORDERING_TABLE_HEAD UINT32_C(0x000c567c)

typedef struct Nba97GameClearOrderingTableRun {
    Nba97GameClearOrderingTableContext* context;
    Nba97GameClearOrderingTableProgress* out;
    Nba97GameClearOrderingTableMachine machine;
} Nba97GameClearOrderingTableRun;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameClearOrderingTableRun* run) {
    run->out->machine = run->machine;
}

static void stop(Nba97GameClearOrderingTableRun* run, uint32_t pc,
    uint32_t address, uint32_t target) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_target = target;
    publish(run);
}

static void set_known(Nba97GameClearOrderingTableWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameClearOrderingTableMachine* machine) {
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

static int validate(Nba97GameClearOrderingTableContext* context,
    Nba97GameClearOrderingTableProgress* out,
    Nba97GameClearOrderingTableRun* run) {
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

/* Enumerate byte carries so partial words retain every exactly invariant byte. */
static Nba97GameClearOrderingTableWord add_words(
    Nba97GameClearOrderingTableWord left,
    Nba97GameClearOrderingTableWord right) {
    Nba97GameClearOrderingTableWord result;
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

static Nba97GameClearOrderingTableWord add_constant(
    Nba97GameClearOrderingTableWord source, uint32_t constant) {
    Nba97GameClearOrderingTableWord value;
    set_known(&value, constant);
    return add_words(source, value);
}

static int spend(Nba97GameClearOrderingTableRun* run) {
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

static void journal(Nba97GameClearOrderingTableRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameClearOrderingTableWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameClearOrderingTableAccess* event =
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

static int locate(Nba97GameClearOrderingTableRun* run, uint32_t address,
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

static int read_value(Nba97GameClearOrderingTableRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    Nba97GameClearOrderingTableWord* value) {
    Nba97GameClearOrderingTableWord loaded = {0, 0};
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
    journal(run, NBA97_GAME_CLEAR_ORDERING_TABLE_READ,
        pc, address, width, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameClearOrderingTableRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    const Nba97GameClearOrderingTableWord* value) {
    Nba97GameClearOrderingTableWord stored = *value;
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
    journal(run, NBA97_GAME_CLEAR_ORDERING_TABLE_STORE,
        pc, address, width, &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int require_address(Nba97GameClearOrderingTableRun* run,
    uint32_t pc, Nba97GameClearOrderingTableWord value,
    uint32_t* address) {
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameClearOrderingTableWord load_lbu(
    Nba97GameClearOrderingTableWord raw) {
    Nba97GameClearOrderingTableWord result;
    result.word = raw.word & 0xffu;
    result.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
    return result;
}

static Nba97GameClearOrderingTableWord sltiu_two(
    Nba97GameClearOrderingTableWord value) {
    Nba97GameClearOrderingTableWord result;
    result.word = value.word < 2u;
    result.known_mask = 0x0eu;
    if (value.known_mask == 0x0fu)
        result.known_mask = 0x0fu;
    return result;
}

static int decide_nonzero(Nba97GameClearOrderingTableRun* run,
    const Nba97GameClearOrderingTableWord* value, uint32_t pc,
    int* branch) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (i * 8u)) & 0xffu)) {
            *branch = 1;
            return NBA97_TEXT_COMPLETE;
        }
    if (value->known_mask == 0x0fu) {
        *branch = 0;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int invoke(Nba97GameClearOrderingTableRun* run, uint32_t pc,
    Nba97GameClearOrderingTableWord target, uint8_t kind,
    uint8_t argument_count, unsigned delay_register,
    Nba97GameClearOrderingTableWord delay_value) {
    Nba97GameClearOrderingTableEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    R(delay_register) = delay_value;
    stop(run, pc, 0, target.word);
    TRY(spend(run));
    ++run->out->call_attempts[kind];
    if (target.known_mask != 0x0fu)
        return NBA97_TEXT_UNKNOWN;
    if (target.word & 3u)
        return NBA97_TEXT_ALIGNMENT_TRAP;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.target = target.word;
    event.operation = run->out->operations;
    event.invocation = run->out->call_attempts[kind];
    event.kind = kind;
    event.argument_count = argument_count;
    publish(run);
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user,
        &run->context->memory, &event, &run->machine);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!machine_valid(&run->machine))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[kind];
    return NBA97_TEXT_COMPLETE;
}

static int restore(Nba97GameClearOrderingTableRun* run, uint32_t pc,
    uint32_t offset, unsigned reg,
    Nba97GameClearOrderingTableWord* reported) {
    uint32_t address;
    TRY(require_address(run, pc,
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), offset), &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_clear_ordering_table(
    Nba97GameClearOrderingTableContext* context,
    Nba97GameClearOrderingTableProgress* out) {
    Nba97GameClearOrderingTableRun storage;
    Nba97GameClearOrderingTableRun* run = &storage;
    Nba97GameClearOrderingTableWord value;
    Nba97GameClearOrderingTableWord branch_value;
    uint32_t address;
    int branch;
    TRY(validate(context, out, run));

    /* 0x80099960..0x80099984: the debug byte is loaded before the frame.
     * BNE's delay-slot ra store executes even when its predicate is unknown. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_value(run, DEBUG_LEVEL_ADDRESS, 1, UINT32_C(0x80099964),
        &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
    out->debug_level = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(require_address(run, UINT32_C(0x8009996c),
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x10u), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8009996c),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A0);
    TRY(require_address(run, UINT32_C(0x80099974),
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x14u), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80099974),
        &R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1)));
    R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1) =
        R(NBA97_MATCH_INITIALIZE_A1);
    R(NBA97_MATCH_INITIALIZE_V0) =
        sltiu_two(R(NBA97_MATCH_INITIALIZE_V0));
    out->debug_predicate = R(NBA97_MATCH_INITIALIZE_V0);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(require_address(run, UINT32_C(0x80099984),
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x80099984),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(decide_nonzero(run, &branch_value, UINT32_C(0x80099980), &branch));

    if (!branch) {
        /* 0x80099988..0x800999A4: resolve and call the live diagnostic. */
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), DEBUG_FORMAT_ADDRESS);
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
        TRY(read_value(run, DEBUG_TARGET_ADDRESS, 4,
            UINT32_C(0x80099998), &R(NBA97_MATCH_INITIALIZE_V0)));
        out->debug_target = R(NBA97_MATCH_INITIALIZE_V0);
        TRY(invoke(run, UINT32_C(0x800999a0),
            R(NBA97_MATCH_INITIALIZE_V0),
            NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG, 3,
            NBA97_MATCH_INITIALIZE_A2,
            R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1)));
    }

    /* 0x800999A8..0x800999C0: the dispatch table is loaded after the
     * diagnostic returns, so all callback mutations remain observable. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_value(run, DISPATCH_TABLE_ADDRESS, 4,
        UINT32_C(0x800999ac), &R(NBA97_MATCH_INITIALIZE_V0)));
    out->dispatch_table = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
    TRY(require_address(run, UINT32_C(0x800999b4),
        add_constant(R(NBA97_MATCH_INITIALIZE_V0), 0x2cu), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x800999b4),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->backend_target = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(invoke(run, UINT32_C(0x800999bc),
        R(NBA97_MATCH_INITIALIZE_V0),
        NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND, 2,
        NBA97_MATCH_INITIALIZE_A1,
        R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1)));

    /* 0x800999C4..0x800999F4: ClearOTagR's result is discarded. Even count
     * zero reaches the head store. All restores use callback-mutable live sp. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x00ffffff));
    R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_S0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800c0000));
    R(NBA97_MATCH_INITIALIZE_V1) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V1), 0x567cu);
    R(NBA97_MATCH_INITIALIZE_V1).word &=
        R(NBA97_MATCH_INITIALIZE_A0).word;
    R(NBA97_MATCH_INITIALIZE_V1).known_mask = 0x0fu;
    out->ordering_table_head = R(NBA97_MATCH_INITIALIZE_V1);
    TRY(require_address(run, UINT32_C(0x800999dc),
        R(NBA97_MATCH_INITIALIZE_V0), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x800999dc),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    out->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(restore(run, UINT32_C(0x800999e0), 0x18u,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x800999e4), 0x14u,
        NBA97_GAME_CLEAR_ORDERING_TABLE_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x800999e8), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x800999f0), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
