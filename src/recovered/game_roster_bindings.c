#include "game_roster_bindings.h"

#include <string.h>

typedef Nba97GameMatchInitializeWord Word;
typedef Nba97GameMatchInitializeRegisters Registers;

typedef struct Nba97GameRosterBindingsRun {
    Nba97GameRosterBindingsContext* context;
    Nba97GameRosterBindingsProgress* out;
    Registers registers;
} Nba97GameRosterBindingsRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define GPR_T1 9
#define GPR_T2 10
#define GPR_T3 11
#define GPR_T4 12
#define GPR_T5 13
#define GPR_T6 14
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameRosterBindingsRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameRosterBindingsRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
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

static void constant(Word* target, uint32_t value) {
    target->word = value;
    target->known_mask = 0x0fu;
}

static void add(Word* target, const Word* left, const Word* right) {
    target->word = left->word + right->word;
    target->known_mask = (uint8_t)(left->known_mask == 0x0fu &&
        right->known_mask == 0x0fu ? 0x0fu : 0);
}

static void add_constant(Word* target, const Word* source, uint32_t value) {
    Word immediate;
    constant(&immediate, value);
    add(target, source, &immediate);
}

static void shift_left(Word* target, const Word* source, unsigned amount) {
    target->word = source->word << amount;
    target->known_mask = (uint8_t)(source->known_mask == 0x0fu ? 0x0fu : 0);
}

static int require_known(Nba97GameRosterBindingsRun* run,
    const Word* value, uint32_t pc) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameRosterBindingsContext* context,
    Nba97GameRosterBindingsProgress* out,
    Nba97GameRosterBindingsRun* run) {
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

static int locate(Nba97GameRosterBindingsRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address);
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

static int observe(Nba97GameRosterBindingsRun* run,
    const Nba97GameRosterBindingsAccess* event) {
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

static int read_value(Nba97GameRosterBindingsRun* run, uint32_t address,
    uint32_t pc, uint8_t width, Word* target) {
    Nba97GameRosterBindingsAccess event;
    uint8_t* data;
    uint8_t* known;
    Word loaded;
    unsigned i;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, address, width, width, pc, &data, &known));
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
    event.address = address;
    event.value = loaded.word;
    event.operation = run->out->operations;
    event.width = width;
    event.known_mask = (uint8_t)(loaded.known_mask & ((1u << width) - 1u));
    event.kind = NBA97_GAME_ROSTER_BINDINGS_READ;
    return observe(run, &event);
}

static int write_value(Nba97GameRosterBindingsRun* run, uint32_t address,
    uint32_t pc, uint8_t width, const Word* source) {
    Nba97GameRosterBindingsAccess event;
    uint8_t* data;
    uint8_t* known;
    uint8_t required_mask = (uint8_t)((1u << width) - 1u);
    unsigned i;
    TRY(locate(run, address, width, width, pc, &data, &known));
    if (!known && (source->known_mask & required_mask) != required_mask)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(source->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((source->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    event.pc = pc;
    event.address = address;
    event.value = source->word & (width == 4 ? UINT32_MAX : 0xffffu);
    event.operation = run->out->operations;
    event.width = width;
    event.known_mask = (uint8_t)(source->known_mask & required_mask);
    event.kind = NBA97_GAME_ROSTER_BINDINGS_STORE;
    return observe(run, &event);
}

static void compare_signed_less(Word* target,
    const Word* left, const Word* right) {
    if (left->known_mask != 0x0fu || right->known_mask != 0x0fu) {
        target->word = (uint32_t)((int32_t)left->word < (int32_t)right->word);
        target->known_mask = 0;
        return;
    }
    constant(target, (uint32_t)((int32_t)left->word < (int32_t)right->word));
}

static int team_count(Nba97GameRosterBindingsRun* run,
    uint32_t team_address, uint32_t load_pc, uint32_t count_pc) {
    Word temp;
    Word* v0 = &REG(run, NBA97_MATCH_INITIALIZE_V0);
    Word* v1 = &REG(run, NBA97_MATCH_INITIALIZE_V1);
    Word* at = &REG(run, NBA97_MATCH_INITIALIZE_AT);
    constant(v0, UINT32_C(0x80020000));
    TRY(read_value(run, team_address, load_pc, 4, v0));
    shift_left(v1, v0, 1);
    add(v1, v1, v0);
    shift_left(v1, v1, 2);
    add(v1, v1, v0);
    shift_left(v1, v1, 3);
    constant(at, UINT32_C(0x80020000));
    add(at, at, v1);
    TRY(require_known(run, at, count_pc));
    temp = *at;
    return read_value(run, temp.word + UINT32_C(0x3aec), count_pc, 1, v0);
}

int nba97_game_roster_bindings(Nba97GameRosterBindingsContext* context,
    Nba97GameRosterBindingsProgress* out) {
    Nba97GameRosterBindingsRun storage;
    Nba97GameRosterBindingsRun* run = &storage;
    Word branch_value;
    int take_branch;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80063D58..0x80063D98: load both selected-team words,
     * cross-link the two team blocks, then store their low halfwords. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A3), 31);
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x8001edf4));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    TRY(read_value(run, UINT32_C(0x80021d74), UINT32_C(0x80063d68), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_A0)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020000));
    TRY(read_value(run, UINT32_C(0x80021d78), UINT32_C(0x80063d70), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_A1)));
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_T0),
        &REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x1d94));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x80024748));
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_V0),
        &REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xc4));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_value(run, UINT32_C(0x8001edf8), UINT32_C(0x80063d88), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_V0)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_value(run, UINT32_C(0x8001eebc), UINT32_C(0x80063d90), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_V1)));
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x80063d94)));
    TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_V1).word,
        UINT32_C(0x80063d94), 2, &REG(run, NBA97_MATCH_INITIALIZE_A0)));
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x80063d98)));
    TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_V1).word + 0xc4u,
        UINT32_C(0x80063d98), 2, &REG(run, NBA97_MATCH_INITIALIZE_A1)));

    /* GAMEONLY 0x80063D9C..0x80063DAC: write all 32 reverse slots. The
     * A2 decrement is the BGEZ delay slot and executes on the final exit. */
    for (;;) {
        TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_T0),
            UINT32_C(0x80063d9c)));
        TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_T0).word,
            UINT32_C(0x80063d9c), 4,
            &REG(run, NBA97_MATCH_INITIALIZE_A2)));
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_T0),
            &REG(run, NBA97_MATCH_INITIALIZE_T0), UINT32_MAX - 3u);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A3),
            &REG(run, NBA97_MATCH_INITIALIZE_A3), UINT32_MAX);
        branch_value = REG(run, NBA97_MATCH_INITIALIZE_A3);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A2),
            &REG(run, NBA97_MATCH_INITIALIZE_A2), UINT32_MAX - 0x67u);
        TRY(require_known(run, &branch_value, UINT32_C(0x80063da8)));
        if ((int32_t)branch_value.word < 0)
            break;
    }

    /* GAMEONLY 0x80063DB0..0x80063DF4: publish the fixed global root and
     * form both mirrored table cursors plus source record cursors. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80015034));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80010000));
    TRY(write_value(run, UINT32_C(0x80015030), UINT32_C(0x80063dbc), 4,
        &REG(run, NBA97_MATCH_INITIALIZE_V1)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A3), 0);
    constant(&REG(run, GPR_T5), UINT32_C(0x8002208c));
    add_constant(&REG(run, GPR_T6),
        &REG(run, GPR_T5), UINT32_C(0x528));
    constant(&REG(run, GPR_T4), 0);
    add_constant(&REG(run, GPR_T1),
        &REG(run, NBA97_MATCH_INITIALIZE_V1), 0x30u);
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020b8c));
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_T0),
        &REG(run, NBA97_MATCH_INITIALIZE_V0), 0x30u);
    constant(&REG(run, GPR_T3), UINT32_C(0x528));
    add_constant(&REG(run, GPR_T2),
        &REG(run, NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffe27e));
    REG(run, NBA97_MATCH_INITIALIZE_A2) = REG(run, NBA97_MATCH_INITIALIZE_V1);
    REG(run, NBA97_MATCH_INITIALIZE_A1) = REG(run, NBA97_MATCH_INITIALIZE_V0);
    REG(run, NBA97_MATCH_INITIALIZE_A0) = REG(run, GPR_T5);

    for (;;) {
        /* GAMEONLY 0x80063DF8..0x80063E48: reload the live home team and
         * count. The in-count J delay slot performs the mirrored store. */
        TRY(team_count(run, UINT32_C(0x80021d74), UINT32_C(0x80063dfc),
            UINT32_C(0x80063e20)));
        compare_signed_less(&REG(run, NBA97_MATCH_INITIALIZE_V0),
            &REG(run, NBA97_MATCH_INITIALIZE_A3),
            &REG(run, NBA97_MATCH_INITIALIZE_V0));
        TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80063e2c)));
        take_branch = REG(run, NBA97_MATCH_INITIALIZE_V0).word == 0;
        if (!take_branch) {
            TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_A1),
                UINT32_C(0x80063e34)));
            TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_A1).word,
                UINT32_C(0x80063e34), 4,
                &REG(run, NBA97_MATCH_INITIALIZE_A0)));
            TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_A2),
                UINT32_C(0x80063e3c)));
            TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_A2).word,
                UINT32_C(0x80063e3c), 4,
                &REG(run, NBA97_MATCH_INITIALIZE_A0)));
        } else {
            TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_A1),
                UINT32_C(0x80063e40)));
            TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_A1).word,
                UINT32_C(0x80063e40), 4,
                &REG(run, GPR_T5)));
            TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_A2),
                UINT32_C(0x80063e44)));
            TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_A2).word,
                UINT32_C(0x80063e44), 4,
                &REG(run, GPR_T5)));
        }
        TRY(require_known(run, &REG(run, GPR_T2),
            UINT32_C(0x80063e48)));
        TRY(write_value(run, REG(run, GPR_T2).word,
            UINT32_C(0x80063e48), 2,
            &REG(run, NBA97_MATCH_INITIALIZE_A3)));

        /* GAMEONLY 0x80063E4C..0x80063E9C: reload the away selection and
         * count. ADdu v0,t3,t5 is the BEQ delay slot on both paths. */
        TRY(team_count(run, UINT32_C(0x80021d78), UINT32_C(0x80063e50),
            UINT32_C(0x80063e74)));
        compare_signed_less(&REG(run, NBA97_MATCH_INITIALIZE_V0),
            &REG(run, NBA97_MATCH_INITIALIZE_A3),
            &REG(run, NBA97_MATCH_INITIALIZE_V0));
        branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
        take_branch = branch_value.word == 0;
        add(&REG(run, NBA97_MATCH_INITIALIZE_V0),
            &REG(run, GPR_T3), &REG(run, GPR_T5));
        TRY(require_known(run, &branch_value, UINT32_C(0x80063e80)));
        if (!take_branch) {
            TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_T0),
                UINT32_C(0x80063e88)));
            TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_T0).word,
                UINT32_C(0x80063e88), 4,
                &REG(run, NBA97_MATCH_INITIALIZE_V0)));
            TRY(require_known(run, &REG(run, GPR_T1),
                UINT32_C(0x80063e90)));
            TRY(write_value(run, REG(run, GPR_T1).word,
                UINT32_C(0x80063e90), 4,
                &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        } else {
            TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_T0),
                UINT32_C(0x80063e94)));
            TRY(write_value(run, REG(run, NBA97_MATCH_INITIALIZE_T0).word,
                UINT32_C(0x80063e94), 4,
                &REG(run, GPR_T6)));
            TRY(require_known(run, &REG(run, GPR_T1),
                UINT32_C(0x80063e98)));
            TRY(write_value(run, REG(run, GPR_T1).word,
                UINT32_C(0x80063e98), 4,
                &REG(run, GPR_T6)));
        }

        /* GAMEONLY 0x80063E9C..0x80063ED0: write the away lineup and
         * advance every cursor. A0 advances in BNE's mandatory delay slot. */
        constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
        add(&REG(run, NBA97_MATCH_INITIALIZE_AT),
            &REG(run, NBA97_MATCH_INITIALIZE_AT),
            &REG(run, GPR_T4));
        TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_AT),
            UINT32_C(0x80063ea4)));
        TRY(write_value(run,
            REG(run, NBA97_MATCH_INITIALIZE_AT).word + UINT32_C(0xffffeece),
            UINT32_C(0x80063ea4), 2,
            &REG(run, NBA97_MATCH_INITIALIZE_A3)));
        add_constant(&REG(run, GPR_T4), &REG(run, GPR_T4), 2);
        add_constant(&REG(run, GPR_T1), &REG(run, GPR_T1), 4);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_T0),
            &REG(run, NBA97_MATCH_INITIALIZE_T0), 4);
        add_constant(&REG(run, GPR_T3), &REG(run, GPR_T3), 0x6e);
        add_constant(&REG(run, GPR_T2), &REG(run, GPR_T2), 2);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A2),
            &REG(run, NBA97_MATCH_INITIALIZE_A2), 4);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A1),
            &REG(run, NBA97_MATCH_INITIALIZE_A1), 4);
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A3),
            &REG(run, NBA97_MATCH_INITIALIZE_A3), 1);
        constant(&branch_value, 12);
        compare_signed_less(&REG(run, NBA97_MATCH_INITIALIZE_V0),
            &REG(run, NBA97_MATCH_INITIALIZE_A3), &branch_value);
        branch_value = REG(run, NBA97_MATCH_INITIALIZE_V0);
        take_branch = REG(run, NBA97_MATCH_INITIALIZE_V0).word != 0;
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            &REG(run, NBA97_MATCH_INITIALIZE_A0), 0x6e);
        TRY(require_known(run, &branch_value, UINT32_C(0x80063ecc)));
        if (!take_branch)
            break;
    }

    /* GAMEONLY 0x80063ED4..0x80063ED8: JR uses the live ra; its delay slot
     * is NOP, so every final computed GPR remains observable. */
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_RA),
        UINT32_C(0x80063ed4)));
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
