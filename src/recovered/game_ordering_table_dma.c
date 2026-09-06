#include "game_ordering_table_dma.h"

#include <string.h>

#define DMA_ADDRESS_POINTER UINT32_C(0x800c56a4)
#define DMA_COUNT_POINTER UINT32_C(0x800c56a8)
#define DMA_CONTROL_POINTER UINT32_C(0x800c56ac)
#define DMA_MASTER_POINTER UINT32_C(0x800c56b0)

typedef struct Nba97GameOrderingTableDmaRun {
    Nba97GameOrderingTableDmaContext* context;
    Nba97GameOrderingTableDmaProgress* out;
    Nba97GameOrderingTableDmaMachine machine;
} Nba97GameOrderingTableDmaRun;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameOrderingTableDmaRun* run) {
    run->out->machine = run->machine;
}

static void stop(Nba97GameOrderingTableDmaRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameOrderingTableDmaWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameOrderingTableDmaMachine* machine) {
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

static int validate(Nba97GameOrderingTableDmaContext* context,
    Nba97GameOrderingTableDmaProgress* out,
    Nba97GameOrderingTableDmaRun* run) {
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

static Nba97GameOrderingTableDmaWord add_words(
    Nba97GameOrderingTableDmaWord left,
    Nba97GameOrderingTableDmaWord right) {
    Nba97GameOrderingTableDmaWord result;
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

static Nba97GameOrderingTableDmaWord add_constant(
    Nba97GameOrderingTableDmaWord source, uint32_t constant) {
    Nba97GameOrderingTableDmaWord value;
    set_known(&value, constant);
    return add_words(source, value);
}

static Nba97GameOrderingTableDmaWord shift_left_two(
    Nba97GameOrderingTableDmaWord source) {
    Nba97GameOrderingTableDmaWord result;
    result.word = source.word << 2u;
    result.known_mask = 0;
    if (source.known_mask & 1u)
        result.known_mask = 1u;
    if ((source.known_mask & 3u) == 3u)
        result.known_mask = (uint8_t)(result.known_mask | 2u);
    if ((source.known_mask & 6u) == 6u)
        result.known_mask = (uint8_t)(result.known_mask | 4u);
    if ((source.known_mask & 12u) == 12u)
        result.known_mask = (uint8_t)(result.known_mask | 8u);
    return result;
}

static Nba97GameOrderingTableDmaWord bit_or(
    Nba97GameOrderingTableDmaWord left,
    Nba97GameOrderingTableDmaWord right) {
    Nba97GameOrderingTableDmaWord result;
    unsigned byte;
    result.word = left.word | right.word;
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        uint32_t shift = byte * 8u;
        uint32_t l = (left.word >> shift) & 0xffu;
        uint32_t r = (right.word >> shift) & 0xffu;
        int lk = (left.known_mask & (1u << byte)) != 0;
        int rk = (right.known_mask & (1u << byte)) != 0;
        if ((lk && rk) || (lk && l == 0xffu) || (rk && r == 0xffu))
            result.known_mask =
                (uint8_t)(result.known_mask | (1u << byte));
    }
    return result;
}

static Nba97GameOrderingTableDmaWord bit_and(
    Nba97GameOrderingTableDmaWord left,
    Nba97GameOrderingTableDmaWord right) {
    Nba97GameOrderingTableDmaWord result;
    unsigned byte;
    result.word = left.word & right.word;
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        uint32_t shift = byte * 8u;
        uint32_t l = (left.word >> shift) & 0xffu;
        uint32_t r = (right.word >> shift) & 0xffu;
        int lk = (left.known_mask & (1u << byte)) != 0;
        int rk = (right.known_mask & (1u << byte)) != 0;
        if ((lk && rk) || (lk && l == 0) || (rk && r == 0))
            result.known_mask =
                (uint8_t)(result.known_mask | (1u << byte));
    }
    return result;
}

static int spend(Nba97GameOrderingTableDmaRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameOrderingTableDmaRun* run, uint8_t kind,
    uint32_t pc, uint32_t address,
    const Nba97GameOrderingTableDmaWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameOrderingTableDmaAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word;
        event->operation = run->out->operations;
        event->width = 4;
        event->known_mask = value->known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameOrderingTableDmaRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (address & 3u)
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        uint64_t offset = (uint64_t)address - region->base;
        if (address < region->base || offset > region->size ||
            4u > region->size - (size_t)offset)
            continue;
        *data = region->data + (size_t)offset;
        *known = region->known ? region->known + (size_t)offset : 0;
        if (*known)
            for (j = 0; j < 4; ++j)
                if ((*known)[j] > 1)
                    return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}

static int read_word(Nba97GameOrderingTableDmaRun* run, uint32_t address,
    uint32_t pc, Nba97GameOrderingTableDmaWord* value) {
    Nba97GameOrderingTableDmaWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_ORDERING_TABLE_DMA_READ, pc, address, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameOrderingTableDmaRun* run, uint32_t address,
    uint32_t pc, const Nba97GameOrderingTableDmaWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_ORDERING_TABLE_DMA_STORE, pc, address, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int require_address(Nba97GameOrderingTableDmaRun* run,
    uint32_t pc, Nba97GameOrderingTableDmaWord value, uint32_t* address) {
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static int decide_zero(Nba97GameOrderingTableDmaRun* run,
    const Nba97GameOrderingTableDmaWord* value, uint32_t pc,
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

static int invoke(Nba97GameOrderingTableDmaRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind) {
    Nba97GameOrderingTableDmaEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    stop(run, pc, 0, entry);
    TRY(spend(run));
    ++run->out->call_attempts[kind];
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = run->out->call_attempts[kind];
    event.kind = kind;
    event.argument_count = 0;
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

static int restore(Nba97GameOrderingTableDmaRun* run, uint32_t pc,
    uint32_t offset, unsigned reg,
    Nba97GameOrderingTableDmaWord* reported) {
    uint32_t address;
    TRY(require_address(run, pc,
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), offset), &address));
    TRY(read_word(run, address, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_ordering_table_dma(Nba97GameOrderingTableDmaContext* context,
    Nba97GameOrderingTableDmaProgress* out) {
    Nba97GameOrderingTableDmaRun storage;
    Nba97GameOrderingTableDmaRun* run = &storage;
    Nba97GameOrderingTableDmaWord branch_value;
    Nba97GameOrderingTableDmaWord zero;
    uint32_t address;
    int is_zero;
    TRY(validate(context, out, run));
    set_known(&zero, 0);

    /* 0x8009A97C..0x8009A9A4: allocate the frame, capture count in s0,
     * resolve the live master-control pointer, then set its bit 27. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(require_address(run, UINT32_C(0x8009a980),
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x10u), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a980),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A1);
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x800c0000));
    TRY(read_word(run, DMA_MASTER_POINTER, UINT32_C(0x8009a98c),
        &R(NBA97_MATCH_INITIALIZE_A1)));
    TRY(require_address(run, UINT32_C(0x8009a990),
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a990),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(require_address(run, UINT32_C(0x8009a994),
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x14u), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a994),
        &R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1)));
    TRY(require_address(run, UINT32_C(0x8009a998),
        R(NBA97_MATCH_INITIALIZE_A1), &address));
    TRY(read_word(run, address, UINT32_C(0x8009a998),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->initial_channel_control = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x08000000));
    R(NBA97_MATCH_INITIALIZE_V0) = bit_or(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    TRY(write_word(run, address, UINT32_C(0x8009a9a4),
        &R(NBA97_MATCH_INITIALIZE_V0)));

    /* 0x8009A9A8..0x8009A9EC: program the four live DMA register pointers.
     * Count is never validated; SLL/ADDIU retains count-zero and wraparound. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_word(run, DMA_CONTROL_POINTER, UINT32_C(0x8009a9ac),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->channel_control_address = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(require_address(run, UINT32_C(0x8009a9b4),
        R(NBA97_MATCH_INITIALIZE_V0), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a9b4), &zero));
    R(NBA97_MATCH_INITIALIZE_V0) =
        shift_left_two(R(NBA97_MATCH_INITIALIZE_S0));
    R(NBA97_MATCH_INITIALIZE_V0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xfffffffc));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800c0000));
    TRY(read_word(run, DMA_ADDRESS_POINTER, UINT32_C(0x8009a9c4),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_A0) = add_words(
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_V0));
    out->transfer_start = R(NBA97_MATCH_INITIALIZE_A0);
    TRY(require_address(run, UINT32_C(0x8009a9cc),
        R(NBA97_MATCH_INITIALIZE_V1), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a9cc),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_word(run, DMA_COUNT_POINTER, UINT32_C(0x8009a9d4),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x11000000));
    TRY(require_address(run, UINT32_C(0x8009a9dc),
        R(NBA97_MATCH_INITIALIZE_V0), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a9dc),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    out->transfer_count = R(NBA97_MATCH_INITIALIZE_S0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_word(run, DMA_CONTROL_POINTER, UINT32_C(0x8009a9e4),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V1).word |= 2u;
    TRY(require_address(run, UINT32_C(0x8009a9ec),
        R(NBA97_MATCH_INITIALIZE_V0), &address));
    TRY(write_word(run, address, UINT32_C(0x8009a9ec),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    out->started_channel_control = R(NBA97_MATCH_INITIALIZE_V1);

    /* 0x8009A9F0 JAL assigns ra=0x8009A9F8 before its NOP delay slot. */
    TRY(invoke(run, UINT32_C(0x8009a9f0), UINT32_C(0x8009bafc),
        NBA97_GAME_ORDERING_TABLE_DMA_START));

    /* 0x8009A9F8..0x8009AA14 reloads live CHCR after the callback. The BEQ
     * delay assigns v0 from live s0 even when masked busy state is unknown. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
    TRY(read_word(run, DMA_CONTROL_POINTER, UINT32_C(0x8009a9fc),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(require_address(run, UINT32_C(0x8009aa04),
        R(NBA97_MATCH_INITIALIZE_V0), &address));
    TRY(read_word(run, address, UINT32_C(0x8009aa04),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x01000000));
    R(NBA97_MATCH_INITIALIZE_V0) = bit_and(
        R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    out->initial_busy_mask = R(NBA97_MATCH_INITIALIZE_V0);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) =
        R(NBA97_MATCH_INITIALIZE_S0); /* 0x8009AA14 delay */
    TRY(decide_zero(run, &branch_value, UINT32_C(0x8009aa10), &is_zero));
    if (!is_zero) {
        set_known(&R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1),
            UINT32_C(0x01000000));
        for (;;) {
            Nba97GameOrderingTableDmaWord child_result;
            /* 0x8009AA1C JAL writes ra=0x8009AA24 before its NOP delay. */
            TRY(invoke(run, UINT32_C(0x8009aa1c),
                UINT32_C(0x8009bb30),
                NBA97_GAME_ORDERING_TABLE_DMA_WAIT));
            ++out->wait_iterations;
            child_result = R(NBA97_MATCH_INITIALIZE_V0);
            out->last_wait_result = child_result;
            set_known(&R(NBA97_MATCH_INITIALIZE_V0),
                UINT32_C(0xffffffff)); /* 0x8009AA28 delay */
            TRY(decide_zero(run, &child_result,
                UINT32_C(0x8009aa24), &is_zero));
            if (!is_zero)
                break;

            /* 0x8009AA2C..0x8009AA48 reloads CHCR every iteration. The AND
             * uses callback-mutable live s1; BNE's delay returns live s0. */
            set_known(&R(NBA97_MATCH_INITIALIZE_V0),
                UINT32_C(0x800c0000));
            TRY(read_word(run, DMA_CONTROL_POINTER,
                UINT32_C(0x8009aa30),
                &R(NBA97_MATCH_INITIALIZE_V0)));
            TRY(require_address(run, UINT32_C(0x8009aa38),
                R(NBA97_MATCH_INITIALIZE_V0), &address));
            TRY(read_word(run, address, UINT32_C(0x8009aa38),
                &R(NBA97_MATCH_INITIALIZE_V0)));
            R(NBA97_MATCH_INITIALIZE_V0) = bit_and(
                R(NBA97_MATCH_INITIALIZE_V0),
                R(NBA97_GAME_CLEAR_ORDERING_TABLE_S1));
            out->last_busy_mask = R(NBA97_MATCH_INITIALIZE_V0);
            branch_value = R(NBA97_MATCH_INITIALIZE_V0);
            R(NBA97_MATCH_INITIALIZE_V0) =
                R(NBA97_MATCH_INITIALIZE_S0); /* 0x8009AA48 delay */
            TRY(decide_zero(run, &branch_value,
                UINT32_C(0x8009aa44), &is_zero));
            if (is_zero)
                break;
        }
    }

    /* 0x8009AA4C..0x8009AA60: report the branch-delay v0, restore through
     * callback-mutable live sp, then consume the possibly unknown JR target. */
    out->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(restore(run, UINT32_C(0x8009aa4c), 0x18u,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x8009aa50), 0x14u,
        NBA97_GAME_CLEAR_ORDERING_TABLE_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x8009aa54), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8009aa5c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
