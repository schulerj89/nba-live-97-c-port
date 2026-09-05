#include "game_match_hot_start.h"

#include <string.h>

typedef Nba97GameMatchInitializeWord Word;
typedef Nba97GameMatchInitializeRegisters Registers;

typedef struct Nba97GameMatchHotStartRun {
    Nba97GameMatchHotStartContext* context;
    Nba97GameMatchHotStartProgress* out;
    Registers registers;
} Nba97GameMatchHotStartRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define GPR_T1 9
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameMatchHotStartRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameMatchHotStartRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int registers_valid(const Registers* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameMatchHotStartContext* context,
    Nba97GameMatchHotStartProgress* out,
    Nba97GameMatchHotStartRun* run) {
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

static void constant(Word* target, uint32_t value) {
    target->word = value;
    target->known_mask = 0x0fu;
}

/* Byte-known addition tracks carry knowledge independently. An unknown low
 * byte does not poison a higher byte when every possible low-byte sum has the
 * same carry, while the concrete word retains ordinary MIPS wraparound. */
static void add(Word* target, const Word* left_source,
    const Word* right_source) {
    Word left = *left_source;
    Word right = *right_source;
    Word result;
    unsigned i;
    unsigned carry_min = 0;
    unsigned carry_max = 0;
    result.word = left.word + right.word;
    result.known_mask = 0;
    for (i = 0; i < 4; ++i) {
        unsigned left_byte = (left.word >> (i * 8u)) & 0xffu;
        unsigned right_byte = (right.word >> (i * 8u)) & 0xffu;
        unsigned left_min = (left.known_mask & (1u << i)) ? left_byte : 0;
        unsigned left_max = (left.known_mask & (1u << i)) ? left_byte : 255;
        unsigned right_min = (right.known_mask & (1u << i)) ? right_byte : 0;
        unsigned right_max = (right.known_mask & (1u << i)) ? right_byte : 255;
        unsigned sum_min = left_min + right_min + carry_min;
        unsigned sum_max = left_max + right_max + carry_max;
        if (sum_min == sum_max)
            result.known_mask = (uint8_t)(result.known_mask | (1u << i));
        carry_min = sum_min >> 8;
        carry_max = sum_max >> 8;
    }
    *target = result;
}

static void add_constant(Word* target, const Word* source, uint32_t value) {
    Word immediate;
    constant(&immediate, value);
    add(target, source, &immediate);
}

static int require_known(Nba97GameMatchHotStartRun* run,
    const Word* value, uint32_t pc) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

/* Return 1 for zero, 0 for nonzero, and -1 when unknown bytes can change the
 * source branch. A known nonzero byte is enough to prove nonzero. */
static int zero_state(const Word* value) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (i * 8u)) & 0xffu))
            return 0;
    return value->known_mask == 0x0fu ? 1 : -1;
}

static int branch_zero(Nba97GameMatchHotStartRun* run, const Word* value,
    uint32_t pc, int* is_zero) {
    int state = zero_state(value);
    if (state < 0) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *is_zero = state;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameMatchHotStartRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address, 0);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
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

static int observe(Nba97GameMatchHotStartRun* run,
    const Nba97GameMatchHotStartAccess* event) {
    int accepted;
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity)
        run->context->access_journal[index] = *event;
    publish(run);
    if (!run->context->observer)
        return NBA97_TEXT_COMPLETE;
    accepted = run->context->observer(run->context->user,
        &run->context->memory, event, &run->registers);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!registers_valid(&run->registers))
        return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
}

static int read_value(Nba97GameMatchHotStartRun* run, const Word* base,
    uint32_t offset, uint32_t pc, uint8_t width, Word* target) {
    Nba97GameMatchHotStartAccess event;
    uint8_t* data;
    uint8_t* known;
    Word address;
    Word loaded;
    uint32_t effective_address;
    unsigned i;
    /* Snapshot the EA before assigning target: LBU v0,7(v0) and
     * LW s0,0xBEC(s0) alias their source base and destination register. */
    add_constant(&address, base, offset);
    TRY(require_known(run, &address, pc));
    effective_address = address.word;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, effective_address, width, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    if (width == 1)
        loaded.known_mask = (uint8_t)(loaded.known_mask | 0x0eu);
    *target = loaded;
    ++run->out->reads;
    event.pc = pc;
    event.address = effective_address;
    event.value = loaded.word;
    event.operation = run->out->operations;
    event.width = width;
    event.known_mask = (uint8_t)(loaded.known_mask & ((1u << width) - 1u));
    event.kind = NBA97_MATCH_HOT_START_READ;
    return observe(run, &event);
}

static int write_value(Nba97GameMatchHotStartRun* run, const Word* address,
    uint32_t pc, uint8_t width, const Word* source) {
    Nba97GameMatchHotStartAccess event;
    uint8_t* data;
    uint8_t* known;
    uint8_t width_mask = (uint8_t)((1u << width) - 1u);
    uint32_t effective_address;
    Word stored;
    unsigned i;
    TRY(require_known(run, address, pc));
    effective_address = address->word;
    stored = *source; /* MIPS samples the source register before the store. */
    TRY(locate(run, effective_address, width, width, pc, &data, &known));
    if (!known && (stored.known_mask & width_mask) != width_mask)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(stored.word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
    }
    ++run->out->stores;
    if (pc == UINT32_C(0x80066fb8))
        ++run->out->prefixes_written;
    event.pc = pc;
    event.address = effective_address;
    event.value = stored.word & (width == 4 ? UINT32_MAX :
        (width == 2 ? 0xffffu : 0xffu));
    event.operation = run->out->operations;
    event.width = width;
    event.known_mask = (uint8_t)(stored.known_mask & width_mask);
    event.kind = NBA97_MATCH_HOT_START_STORE;
    return observe(run, &event);
}

static int write_at(Nba97GameMatchHotStartRun* run, uint32_t address,
    uint32_t pc, uint8_t width, const Word* source) {
    Word location;
    constant(&location, address);
    return write_value(run, &location, pc, width, source);
}

static int invoke(Nba97GameMatchHotStartRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    uint32_t delay_slot_pc, unsigned delay_register,
    uint32_t delay_value) {
    Nba97GameMatchHotStartEvent event;
    int accepted;
    REG(run, NBA97_MATCH_INITIALIZE_RA).word = pc + 8u;
    REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask = 0x0fu;
    if (delay_register < NBA97_MATCH_INITIALIZE_REGISTER_COUNT)
        constant(&REG(run, delay_register), delay_value);
    stop(run, pc, 0, entry);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = delay_slot_pc;
    event.entry = entry;
    event.operation = run->out->operations;
    event.kind = kind;
    event.argument_count = argument_count;
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

int nba97_game_match_hot_start(Nba97GameMatchHotStartContext* context,
    Nba97GameMatchHotStartProgress* out) {
    Nba97GameMatchHotStartRun storage;
    Nba97GameMatchHotStartRun* run = &storage;
    Word address;
    Word branch_value;
    Word zero;
    int is_zero;
    constant(&zero, 0);
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80066F88..0x80066FB4: build the live frame and the three
     * retained-table cursors before saving ra, s1, and s0 in source order. */
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_SP),
        &REG(run, NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = REG(run, NBA97_MATCH_INITIALIZE_SP).word;
    constant(&REG(run, GPR_T1), 0);
    constant(&REG(run, NBA97_MATCH_INITIALIZE_T0), 0);
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x800170c8));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x8001ec98));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x800fe920));
    add_constant(&address, &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x18u);
    TRY(write_value(run, &address, UINT32_C(0x80066fac), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_RA)));
    add_constant(&address, &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x14u);
    TRY(write_value(run, &address, UINT32_C(0x80066fb0), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_S0 + 1)));
    add_constant(&address, &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x10u);
    TRY(write_value(run, &address, UINT32_C(0x80066fb4), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_S0)));

    /* GAMEONLY 0x80066FB8..0x8006701C: each iteration stores the current
     * low-halfword prefix, reads left then right optional payload bytes, and
     * adds the signed-SLT-selected maximum. The BNE delay advances a1. */
    for (;;) {
        TRY(write_value(run, &REG(run, NBA97_MATCH_INITIALIZE_A1),
            UINT32_C(0x80066fb8), 2, &REG(run, GPR_T1)));
        TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_A2), 0,
            UINT32_C(0x80066fbc), 4,
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        TRY(branch_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80066fc4), &is_zero));
        if (is_zero) {
            constant(&REG(run, NBA97_MATCH_INITIALIZE_V0), 0);
        } else {
            TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_V0), 7u,
                UINT32_C(0x80066fcc), 1,
                &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        }
        TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_A3), 0,
            UINT32_C(0x80066fdc), 4,
            &REG(run, NBA97_MATCH_INITIALIZE_V1)));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), 0); /* BEQ delay. */
        TRY(branch_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V1),
            UINT32_C(0x80066fe4), &is_zero));
        if (!is_zero) {
            TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_V1), 7u,
                UINT32_C(0x80066fec), 1,
                &REG(run, NBA97_MATCH_INITIALIZE_A0)));
        }
        REG(run, NBA97_MATCH_INITIALIZE_V1) =
            REG(run, NBA97_MATCH_INITIALIZE_V0);
        if (REG(run, NBA97_MATCH_INITIALIZE_V1).known_mask != 0x0fu ||
            REG(run, NBA97_MATCH_INITIALIZE_A0).known_mask != 0x0fu) {
            REG(run, NBA97_MATCH_INITIALIZE_V0).word =
                (uint32_t)((int32_t)REG(run, NBA97_MATCH_INITIALIZE_V1).word <
                (int32_t)REG(run, NBA97_MATCH_INITIALIZE_A0).word);
            REG(run, NBA97_MATCH_INITIALIZE_V0).known_mask = 0x0eu;
        } else {
            constant(&REG(run, NBA97_MATCH_INITIALIZE_V0),
                (uint32_t)((int32_t)REG(run,
                    NBA97_MATCH_INITIALIZE_V1).word <
                    (int32_t)REG(run, NBA97_MATCH_INITIALIZE_A0).word));
        }
        TRY(branch_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80066ff8), &is_zero));
        if (!is_zero)
            REG(run, NBA97_MATCH_INITIALIZE_V1) =
                REG(run, NBA97_MATCH_INITIALIZE_A0);
        add(&REG(run, GPR_T1), &REG(run, GPR_T1),
            &REG(run, NBA97_MATCH_INITIALIZE_V1));
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A3),
            &REG(run, NBA97_MATCH_INITIALIZE_A3), 4u);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A2),
            &REG(run, NBA97_MATCH_INITIALIZE_A2), 4u);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_T0),
            &REG(run, NBA97_MATCH_INITIALIZE_T0), 1u);
        branch_value.word = (uint32_t)((int32_t)REG(run,
            NBA97_MATCH_INITIALIZE_T0).word < 84);
        branch_value.known_mask = REG(run,
            NBA97_MATCH_INITIALIZE_T0).known_mask == 0x0fu ? 0x0fu : 0x0eu;
        REG(run, NBA97_MATCH_INITIALIZE_V0) = branch_value;
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A1),
            &REG(run, NBA97_MATCH_INITIALIZE_A1), 2u); /* BNE delay. */
        TRY(branch_zero(run, &branch_value, UINT32_C(0x80067018), &is_zero));
        if (is_zero)
            break;
    }

    /* GAMEONLY 0x80067020..0x80067038: load the live selected root, pass
     * *s0 and 0x4E, and assign s1=1 in the first JAL delay slot. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80020000));
    TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_S0), 0xbecu,
        UINT32_C(0x80067024), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_S0)));
    TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_S0), 0,
        UINT32_C(0x8006702c), 4, &REG(run, NBA97_MATCH_INITIALIZE_A0)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0x4eu);
    TRY(invoke(run, UINT32_C(0x80067034), UINT32_C(0x80051ed8),
        NBA97_MATCH_HOT_START_CHILD_80051ED8, 2,
        UINT32_C(0x80067038), NBA97_MATCH_INITIALIZE_S0 + 1, 1));

    /* GAMEONLY 0x8006703C..0x80067070: the loader do-loop republishes zero,
     * stores raw v0, writes callback-live s1, and retries exactly on v0==0. */
    for (;;) {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x800275b8));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1),
            UINT32_C(0x800c6400));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_AT),
            UINT32_C(0x800d0000));
        TRY(write_at(run, UINT32_C(0x800d7af8), UINT32_C(0x80067050), 4,
            &zero));
        TRY(invoke(run, UINT32_C(0x80067054), UINT32_C(0x800a72bc),
            NBA97_MATCH_HOT_START_CHILD_800A72BC, 2,
            UINT32_C(0x80067058), NBA97_MATCH_INITIALIZE_REGISTER_COUNT, 0));
        ++out->retry_attempts;
        constant(&REG(run, NBA97_MATCH_INITIALIZE_AT),
            UINT32_C(0x80100000));
        TRY(write_at(run, UINT32_C(0x800fe91c), UINT32_C(0x80067060), 4,
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_AT),
            UINT32_C(0x800d0000));
        TRY(write_at(run, UINT32_C(0x800d7af8), UINT32_C(0x80067068), 4,
            &REG(run, NBA97_MATCH_INITIALIZE_S0 + 1)));
        TRY(branch_zero(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x8006706c), &is_zero));
        if (!is_zero)
            break;
    }

    /* GAMEONLY 0x80067074..0x8006708C: clear the source halfword, then use
     * callback-live s0 for both dereferences and the unsigned +9 byte. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_at(run, UINT32_C(0x8002148c), UINT32_C(0x80067078), 2, &zero));
    TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_S0), 0x20u,
        UINT32_C(0x8006707c), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_V0)));
    TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_S0), 0,
        UINT32_C(0x80067080), 4, &REG(run, NBA97_MATCH_INITIALIZE_A0)));
    TRY(read_value(run, &REG(run, NBA97_MATCH_INITIALIZE_V0), 9u,
        UINT32_C(0x80067084), 1,
        &REG(run, NBA97_MATCH_INITIALIZE_A1)));
    TRY(invoke(run, UINT32_C(0x80067088), UINT32_C(0x80051ed8),
        NBA97_MATCH_HOT_START_CHILD_80051ED8, 2,
        UINT32_C(0x8006708c), NBA97_MATCH_INITIALIZE_REGISTER_COUNT, 0));

    /* GAMEONLY 0x80067090..0x800670A4: each saved register reload addresses
     * the callback-live sp; JR consumes the restored ra after sp advances. */
    add_constant(&address, &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x18u);
    TRY(read_value(run, &address, 0, UINT32_C(0x80067090), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = REG(run, NBA97_MATCH_INITIALIZE_RA);
    add_constant(&address, &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x14u);
    TRY(read_value(run, &address, 0, UINT32_C(0x80067094), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_S0 + 1)));
    add_constant(&address, &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x10u);
    TRY(read_value(run, &address, 0, UINT32_C(0x80067098), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_S0)));
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_SP),
        &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x20u);
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_RA),
        UINT32_C(0x800670a0)));

    out->completed = 1;
    out->stopped_pc = 0;
    out->stopped_address = 0;
    out->stopped_entry = 0;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}
