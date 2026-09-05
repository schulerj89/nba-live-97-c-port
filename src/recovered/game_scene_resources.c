#include "game_scene_resources.h"

#include <string.h>

typedef Nba97GameSceneResourcesWord Word;
typedef Nba97GameSceneResourcesRegisters Registers;

typedef struct Nba97GameSceneResourcesRun {
    Nba97GameSceneResourcesContext* context;
    Nba97GameSceneResourcesProgress* out;
    Registers registers;
} Nba97GameSceneResourcesRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define GPR_S1 17
#define GPR_S2 18
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameSceneResourcesRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameSceneResourcesRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameSceneResourcesRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
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

static uint32_t known_bits(const Word* value) {
    uint32_t bits = 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (value->known_mask & (1u << i))
            bits |= UINT32_C(0xff) << (i * 8u);
    return bits;
}

static uint8_t known_bytes(uint32_t bits) {
    uint8_t mask = 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (((bits >> (i * 8u)) & 0xffu) == 0xffu)
            mask = (uint8_t)(mask | (1u << i));
    return mask;
}

/* ADDU/ADDIU carry dependencies flow from low bytes upward. This retains a
 * known low prefix even when later bytes are unknown. */
static void add(Word* target, const Word* left, const Word* right) {
    uint8_t mask = 0;
    unsigned i;
    target->word = left->word + right->word;
    for (i = 0; i < 4; ++i) {
        uint8_t prefix = (uint8_t)((1u << (i + 1u)) - 1u);
        if ((left->known_mask & prefix) == prefix &&
            (right->known_mask & prefix) == prefix)
            mask = (uint8_t)(mask | (1u << i));
    }
    target->known_mask = mask;
}

static void add_constant(Word* target, const Word* source, uint32_t value) {
    Word immediate;
    constant(&immediate, value);
    add(target, source, &immediate);
}

static void shift_left(Word* target, const Word* source, unsigned amount) {
    uint32_t bits = known_bits(source);
    target->word = source->word << amount;
    bits = amount ? (bits << amount) | ((UINT32_C(1) << amount) - 1u) : bits;
    target->known_mask = known_bytes(bits);
}

static void signed_less_immediate(Word* target, const Word* source,
    int32_t immediate) {
    target->word = (uint32_t)((int32_t)source->word < immediate);
    target->known_mask = source->known_mask == 0x0fu ? 0x0fu : 0;
}

static int require_known(Nba97GameSceneResourcesRun* run,
    const Word* value, uint32_t pc) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameSceneResourcesContext* context,
    Nba97GameSceneResourcesProgress* out,
    Nba97GameSceneResourcesRun* run) {
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

static int locate(Nba97GameSceneResourcesRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    unsigned j;
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

static void journal(Nba97GameSceneResourcesRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, const Word* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameSceneResourcesAccess* event =
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

static int read_word(Nba97GameSceneResourcesRun* run, uint32_t address,
    uint32_t pc, Word* target) {
    uint8_t* data;
    uint8_t* known;
    Word value;
    unsigned i;
    value.word = 0;
    value.known_mask = 0;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        value.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            value.known_mask = (uint8_t)(value.known_mask | (1u << i));
    }
    *target = value;
    ++run->out->reads;
    journal(run, NBA97_GAME_SCENE_RESOURCES_READ, pc, address, &value);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameSceneResourcesRun* run, uint32_t address,
    uint32_t pc, const Word* source) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && source->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(source->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((source->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_SCENE_RESOURCES_STORE, pc, address, source);
    return NBA97_TEXT_COMPLETE;
}

static uint8_t call_kind(uint32_t entry) {
    switch (entry) {
    case UINT32_C(0x800536a0): return NBA97_GAME_SCENE_RESOURCES_CHILD_800536A0;
    case UINT32_C(0x8004d490): return NBA97_GAME_SCENE_RESOURCES_CHILD_8004D490;
    case UINT32_C(0x80029bfc): return NBA97_GAME_SCENE_RESOURCES_CHILD_80029BFC;
    case UINT32_C(0x80029bcc): return NBA97_GAME_SCENE_RESOURCES_CHILD_80029BCC;
    case UINT32_C(0x800516e4): return NBA97_GAME_SCENE_RESOURCES_CHILD_800516E4;
    case UINT32_C(0x80029bd4): return NBA97_GAME_SCENE_RESOURCES_CHILD_80029BD4;
    case UINT32_C(0x800a3fec): return NBA97_GAME_SCENE_RESOURCES_CHILD_800A3FEC;
    case UINT32_C(0x80051294): return NBA97_GAME_SCENE_RESOURCES_CHILD_80051294;
    case UINT32_C(0x80090160): return NBA97_GAME_SCENE_RESOURCES_CHILD_80090160;
    case UINT32_C(0x8004dc08): return NBA97_GAME_SCENE_RESOURCES_CHILD_8004DC08;
    case UINT32_C(0x8004fd38): return NBA97_GAME_SCENE_RESOURCES_CHILD_8004FD38;
    case UINT32_C(0x800994f4): return NBA97_GAME_SCENE_RESOURCES_CHILD_800994F4;
    case UINT32_C(0x80090698): return NBA97_GAME_SCENE_RESOURCES_CHILD_80090698;
    case UINT32_C(0x8004fd48): return NBA97_GAME_SCENE_RESOURCES_CHILD_8004FD48;
    case UINT32_C(0x800504a8): return NBA97_GAME_SCENE_RESOURCES_CHILD_800504A8;
    case UINT32_C(0x80050dd0): return NBA97_GAME_SCENE_RESOURCES_CHILD_80050DD0;
    case UINT32_C(0x80050dc8): return NBA97_GAME_SCENE_RESOURCES_CHILD_80050DC8;
    default: return NBA97_GAME_SCENE_RESOURCES_CHILD_800479B8;
    }
}

static void begin_call(Nba97GameSceneResourcesRun* run, uint32_t pc) {
    constant(&REG(run, NBA97_MATCH_INITIALIZE_RA), pc + 8u);
}

static int enter_call(Nba97GameSceneResourcesRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count) {
    Nba97GameSceneResourcesEvent event;
    int accepted;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.kind = call_kind(entry);
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

static int branch_known(Nba97GameSceneResourcesRun* run,
    const Word* condition, uint32_t pc, int* nonzero) {
    TRY(require_known(run, condition, pc));
    *nonzero = condition->word != 0;
    return NBA97_TEXT_COMPLETE;
}

static int load_fixed(Nba97GameSceneResourcesRun* run, unsigned reg,
    uint32_t address, uint32_t pc) {
    /* Source LUI uses the relocation-adjusted upper half when LW's signed
     * displacement is negative. Keep that prefix visible if the access stops. */
    constant(&REG(run, reg),
        (address + UINT32_C(0x8000)) & UINT32_C(0xffff0000));
    return read_word(run, address, pc, &REG(run, reg));
}

static int store_fixed(Nba97GameSceneResourcesRun* run, unsigned at_base,
    uint32_t address, uint32_t pc, const Word* source) {
    constant(&REG(run, NBA97_MATCH_INITIALIZE_AT), at_base);
    return write_word(run, address, pc, source);
}

static int call_nop(Nba97GameSceneResourcesRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count) {
    begin_call(run, pc);
    return enter_call(run, pc, entry, argument_count);
}

static int call_clear(Nba97GameSceneResourcesRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count, unsigned reg) {
    begin_call(run, pc);
    constant(&REG(run, reg), 0);
    return enter_call(run, pc, entry, argument_count);
}

static int load_mode_and_branch(Nba97GameSceneResourcesRun* run,
    uint32_t load_pc, uint32_t branch_pc, int delay_a0_700, int* nonzero) {
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_V0,
        UINT32_C(0x800eb678), load_pc));
    if (delay_a0_700)
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), 700);
    return branch_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        branch_pc, nonzero);
}

static int load_team_pointer(Nba97GameSceneResourcesRun* run,
    uint32_t team_address, uint32_t table_base, uint32_t load_pc,
    uint32_t table_pc, unsigned index_register) {
    Word* index = &REG(run, index_register);
    Word* at = &REG(run, NBA97_MATCH_INITIALIZE_AT);
    TRY(load_fixed(run, index_register, team_address, load_pc));
    shift_left(index, index, 2);
    constant(at, UINT32_C(0x800b0000));
    add(at, at, index);
    TRY(require_known(run, at, table_pc));
    return read_word(run, at->word + table_base, table_pc,
        &REG(run, NBA97_MATCH_INITIALIZE_A0));
}

static int lookup_call(Nba97GameSceneResourcesRun* run, uint32_t pc,
    const Word* index, int increment_s0) {
    REG(run, NBA97_MATCH_INITIALIZE_A1) = *index;
    begin_call(run, pc);
    if (increment_s0)
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_S0),
            &REG(run, NBA97_MATCH_INITIALIZE_S0), 1);
    return enter_call(run, pc, UINT32_C(0x800a3fec), 2);
}

int nba97_game_scene_resources(Nba97GameSceneResourcesContext* context,
    Nba97GameSceneResourcesProgress* out) {
    Nba97GameSceneResourcesRun storage;
    Nba97GameSceneResourcesRun* run = &storage;
    Word branch;
    Word zero;
    int alternate;
    TRY(validate(context, out, run));
    constant(&zero, 0);

    /* GAMEONLY 0x80052C20..0x80052C34: create the 0x20-byte frame. JAL
     * assigns ra before its delay-slot store of the incoming s0. */
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_SP),
        &REG(run, NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = REG(run, NBA97_MATCH_INITIALIZE_SP).word;
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0x80052c24)));
    TRY(write_word(run, out->frame_stack_pointer + 0x1cu,
        UINT32_C(0x80052c24), &REG(run, NBA97_MATCH_INITIALIZE_RA)));
    TRY(write_word(run, out->frame_stack_pointer + 0x18u,
        UINT32_C(0x80052c28), &REG(run, GPR_S2)));
    TRY(write_word(run, out->frame_stack_pointer + 0x14u,
        UINT32_C(0x80052c2c), &REG(run, GPR_S1)));
    begin_call(run, UINT32_C(0x80052c30));
    TRY(write_word(run, out->frame_stack_pointer + 0x10u,
        UINT32_C(0x80052c34), &REG(run, NBA97_MATCH_INITIALIZE_S0)));
    TRY(enter_call(run, UINT32_C(0x80052c30),
        UINT32_C(0x800536a0), 0));

    /* 0x80052C38..0x80052CD4 initializes scene flags, then rereads the live
     * mode after each child before selecting the normal or bracketed path. */
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_V0,
        UINT32_C(0x800eb678), UINT32_C(0x80052c3c)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V1), 1);
    TRY(store_fixed(run, UINT32_C(0x800b0000), UINT32_C(0x800b72dc),
        UINT32_C(0x80052c48), &REG(run, NBA97_MATCH_INITIALIZE_V1)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffffd));
    TRY(store_fixed(run, UINT32_C(0x80100000), UINT32_C(0x800fb820),
        UINT32_C(0x80052c54), &zero));
    TRY(store_fixed(run, UINT32_C(0x80100000), UINT32_C(0x800fac20),
        UINT32_C(0x80052c5c), &REG(run, NBA97_MATCH_INITIALIZE_V1)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), 700); /* BNE delay. */
    TRY(branch_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80052c60), &alternate));
    if (!alternate) {
        TRY(call_nop(run, UINT32_C(0x80052c68),
            UINT32_C(0x8004d490), 1));
        TRY(load_mode_and_branch(run, UINT32_C(0x80052c74),
            UINT32_C(0x80052c7c), 1, &alternate));
        if (!alternate) {
            constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
                UINT32_C(0x8002639c));
            constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0x20);
            TRY(call_nop(run, UINT32_C(0x80052c8c),
                UINT32_C(0x80029bfc), 2));
            TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_V1,
                UINT32_C(0x800eb678), UINT32_C(0x80052c98)));
            TRY(store_fixed(run, UINT32_C(0x80100000),
                UINT32_C(0x800f9fc0), UINT32_C(0x80052ca0),
                &REG(run, NBA97_MATCH_INITIALIZE_V0)));
            constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), 700);
            TRY(branch_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V1),
                UINT32_C(0x80052ca4), &alternate));
            if (!alternate) {
                TRY(call_nop(run, UINT32_C(0x80052cd0),
                    UINT32_C(0x800516e4), 0));
            }
        }
    }
    if (alternate) {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 480);
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A2), 0);
        TRY(call_nop(run, UINT32_C(0x80052cb0),
            UINT32_C(0x80029bcc), 3));
        TRY(call_nop(run, UINT32_C(0x80052cb8),
            UINT32_C(0x800516e4), 0));
        TRY(call_nop(run, UINT32_C(0x80052cc0),
            UINT32_C(0x80029bd4), 0));
    }

    /* 0x80052CD8..0x80052E7C reloads mode once, then builds either both
     * ten-entry team lookup tables or only the home table. Child mutations of
     * s0/s1/s2 remain live in every loop test and destination access. */
    TRY(load_mode_and_branch(run, UINT32_C(0x80052cdc),
        UINT32_C(0x80052ce4), 1, &alternate));
    if (!alternate) {
        TRY(load_team_pointer(run, UINT32_C(0x80021d74), 0x7394u,
            UINT32_C(0x80052cf0), UINT32_C(0x80052d04),
            NBA97_MATCH_INITIALIZE_V0));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(call_nop(run, UINT32_C(0x80052d08),
            UINT32_C(0x80029bfc), 2));
        TRY(load_team_pointer(run, UINT32_C(0x80021d78), 0x741cu,
            UINT32_C(0x80052d14), UINT32_C(0x80052d28),
            NBA97_MATCH_INITIALIZE_V1));
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800f0edc), UINT32_C(0x80052d30),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(call_nop(run, UINT32_C(0x80052d34),
            UINT32_C(0x80029bfc), 2));
        TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
            UINT32_C(0x800f0edc), UINT32_C(0x80052d40)));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_S0), 0);
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800f0fac), UINT32_C(0x80052d4c),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(call_nop(run, UINT32_C(0x80052d50),
            UINT32_C(0x800a3fec), 2));
        TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
            UINT32_C(0x800f0fac), UINT32_C(0x80052d5c)));
        constant(&REG(run, GPR_S2), UINT32_C(0x800fb154));
        constant(&REG(run, GPR_S1), UINT32_C(0x800fac24));
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800ebc38), UINT32_C(0x80052d74),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(call_nop(run, UINT32_C(0x80052d78),
            UINT32_C(0x800a3fec), 2));
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800f0f64), UINT32_C(0x80052d84),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        for (;;) {
            TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
                UINT32_C(0x800f0edc), UINT32_C(0x80052d8c)));
            branch = REG(run, NBA97_MATCH_INITIALIZE_S0);
            TRY(lookup_call(run, UINT32_C(0x80052d90), &branch, 0));
            TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
                UINT32_C(0x800f0fac), UINT32_C(0x80052d9c)));
            REG(run, NBA97_MATCH_INITIALIZE_A1) =
                REG(run, NBA97_MATCH_INITIALIZE_S0);
            TRY(require_known(run, &REG(run, GPR_S1),
                UINT32_C(0x80052da4)));
            TRY(write_word(run, REG(run, GPR_S1).word,
                UINT32_C(0x80052da4), &REG(run, NBA97_MATCH_INITIALIZE_V0)));
            add_constant(&REG(run, GPR_S1), &REG(run, GPR_S1), 4);
            begin_call(run, UINT32_C(0x80052dac));
            add_constant(&REG(run, NBA97_MATCH_INITIALIZE_S0),
                &REG(run, NBA97_MATCH_INITIALIZE_S0), 1);
            TRY(enter_call(run, UINT32_C(0x80052dac),
                UINT32_C(0x800a3fec), 2));
            TRY(require_known(run, &REG(run, GPR_S2),
                UINT32_C(0x80052db4)));
            TRY(write_word(run, REG(run, GPR_S2).word,
                UINT32_C(0x80052db4), &REG(run, NBA97_MATCH_INITIALIZE_V0)));
            signed_less_immediate(&REG(run, NBA97_MATCH_INITIALIZE_V0),
                &REG(run, NBA97_MATCH_INITIALIZE_S0), 10);
            branch = REG(run, NBA97_MATCH_INITIALIZE_V0);
            add_constant(&REG(run, GPR_S2), &REG(run, GPR_S2), 4);
            TRY(branch_known(run, &branch, UINT32_C(0x80052dbc),
                &alternate));
            if (!alternate)
                break;
        }
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x800263ac));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(call_nop(run, UINT32_C(0x80052dcc),
            UINT32_C(0x80029bfc), 2));
        TRY(store_fixed(run, UINT32_C(0x80100000),
            UINT32_C(0x800fabcc), UINT32_C(0x80052dd8),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        REG(run, GPR_S2) = REG(run, NBA97_MATCH_INITIALIZE_V0); /* J delay. */
    } else {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 480);
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A2), 0);
        TRY(call_nop(run, UINT32_C(0x80052de8),
            UINT32_C(0x80029bcc), 3));
        TRY(load_team_pointer(run, UINT32_C(0x80021d74), 0x7394u,
            UINT32_C(0x80052df4), UINT32_C(0x80052e08),
            NBA97_MATCH_INITIALIZE_V0));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0);
        constant(&REG(run, GPR_S1), UINT32_C(0x800fac24));
        begin_call(run, UINT32_C(0x80052e18));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_S0), 0);
        TRY(enter_call(run, UINT32_C(0x80052e18),
            UINT32_C(0x80029bfc), 2));
        REG(run, NBA97_MATCH_INITIALIZE_A0) =
            REG(run, NBA97_MATCH_INITIALIZE_V0);
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800f0edc), UINT32_C(0x80052e28),
            &REG(run, NBA97_MATCH_INITIALIZE_A0)));
        TRY(call_clear(run, UINT32_C(0x80052e2c),
            UINT32_C(0x800a3fec), 2, NBA97_MATCH_INITIALIZE_A1));
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800ebc38), UINT32_C(0x80052e38),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        for (;;) {
            TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
                UINT32_C(0x800f0edc), UINT32_C(0x80052e40)));
            REG(run, NBA97_MATCH_INITIALIZE_A1) =
                REG(run, NBA97_MATCH_INITIALIZE_S0);
            begin_call(run, UINT32_C(0x80052e48));
            add_constant(&REG(run, NBA97_MATCH_INITIALIZE_S0),
                &REG(run, NBA97_MATCH_INITIALIZE_S0), 1);
            TRY(enter_call(run, UINT32_C(0x80052e48),
                UINT32_C(0x800a3fec), 2));
            TRY(require_known(run, &REG(run, GPR_S1),
                UINT32_C(0x80052e50)));
            TRY(write_word(run, REG(run, GPR_S1).word,
                UINT32_C(0x80052e50), &REG(run, NBA97_MATCH_INITIALIZE_V0)));
            signed_less_immediate(&REG(run, NBA97_MATCH_INITIALIZE_V0),
                &REG(run, NBA97_MATCH_INITIALIZE_S0), 10);
            branch = REG(run, NBA97_MATCH_INITIALIZE_V0);
            add_constant(&REG(run, GPR_S1), &REG(run, GPR_S1), 4);
            TRY(branch_known(run, &branch, UINT32_C(0x80052e58),
                &alternate));
            if (!alternate)
                break;
        }
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x800263ac));
        TRY(call_clear(run, UINT32_C(0x80052e68),
            UINT32_C(0x80029bfc), 2, NBA97_MATCH_INITIALIZE_A1));
        TRY(store_fixed(run, UINT32_C(0x80100000),
            UINT32_C(0x800fabcc), UINT32_C(0x80052e74),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        begin_call(run, UINT32_C(0x80052e78));
        REG(run, GPR_S2) = REG(run, NBA97_MATCH_INITIALIZE_V0);
        TRY(enter_call(run, UINT32_C(0x80052e78),
            UINT32_C(0x80029bd4), 0));
    }

    /* 0x80052E80..0x80052EA8 expands all 26 letter resources. The lookup
     * child can alter the signed loop counter, resource root, or cursor. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_S0), 0);
    constant(&REG(run, GPR_S1), UINT32_C(0x800feca8));
    for (;;) {
        REG(run, NBA97_MATCH_INITIALIZE_A0) = REG(run, GPR_S2);
        branch = REG(run, NBA97_MATCH_INITIALIZE_S0);
        TRY(lookup_call(run, UINT32_C(0x80052e90), &branch, 0));
        TRY(require_known(run, &REG(run, GPR_S1),
            UINT32_C(0x80052e98)));
        TRY(write_word(run, REG(run, GPR_S1).word,
            UINT32_C(0x80052e98), &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        add_constant(&REG(run, NBA97_MATCH_INITIALIZE_S0),
            &REG(run, NBA97_MATCH_INITIALIZE_S0), 1);
        signed_less_immediate(&REG(run, NBA97_MATCH_INITIALIZE_V0),
            &REG(run, NBA97_MATCH_INITIALIZE_S0), 26);
        branch = REG(run, NBA97_MATCH_INITIALIZE_V0);
        add_constant(&REG(run, GPR_S1), &REG(run, GPR_S1), 4);
        TRY(branch_known(run, &branch, UINT32_C(0x80052ea4), &alternate));
        if (!alternate)
            break;
    }

    /* 0x80052EAC..0x80052F1C clears player state and loads the mode-selected
     * player archive, with the alternate load bracketed by presentation calls. */
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_V0,
        UINT32_C(0x800eb678), UINT32_C(0x80052eb0)));
    TRY(store_fixed(run, UINT32_C(0x800e0000), UINT32_C(0x800d9284),
        UINT32_C(0x80052eb8), &zero));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), 700);
    TRY(branch_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x80052ebc), &alternate));
    if (!alternate) {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x800263bc));
        TRY(call_clear(run, UINT32_C(0x80052ecc),
            UINT32_C(0x80029bfc), 2, NBA97_MATCH_INITIALIZE_A1));
        TRY(store_fixed(run, UINT32_C(0x80100000),
            UINT32_C(0x801041a0), UINT32_C(0x80052ed8),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        TRY(call_nop(run, UINT32_C(0x80052edc),
            UINT32_C(0x80051294), 0));
    } else {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 480);
        TRY(call_clear(run, UINT32_C(0x80052ef0),
            UINT32_C(0x80029bcc), 3, NBA97_MATCH_INITIALIZE_A2));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x800263cc));
        TRY(call_clear(run, UINT32_C(0x80052f00),
            UINT32_C(0x80029bfc), 2, NBA97_MATCH_INITIALIZE_A1));
        TRY(store_fixed(run, UINT32_C(0x80100000),
            UINT32_C(0x801041a0), UINT32_C(0x80052f0c),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
        TRY(call_nop(run, UINT32_C(0x80052f10),
            UINT32_C(0x80051294), 0));
        TRY(call_nop(run, UINT32_C(0x80052f18),
            UINT32_C(0x80029bd4), 0));
    }

    /* 0x80052F20..0x80052F90 allocates named head and palette buffers and
     * performs the mode-selected head setup, preserving each raw return. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800263dc));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0x970);
    TRY(call_clear(run, UINT32_C(0x80052f2c),
        UINT32_C(0x80090160), 3, NBA97_MATCH_INITIALIZE_A2));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800263e8));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 0x210);
    TRY(store_fixed(run, UINT32_C(0x80100000),
        UINT32_C(0x800fdb34), UINT32_C(0x80052f44),
        &REG(run, NBA97_MATCH_INITIALIZE_V0)));
    TRY(call_clear(run, UINT32_C(0x80052f48),
        UINT32_C(0x80090160), 3, NBA97_MATCH_INITIALIZE_A2));
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_V1,
        UINT32_C(0x800eb678), UINT32_C(0x80052f54)));
    TRY(store_fixed(run, UINT32_C(0x800e0000),
        UINT32_C(0x800dcbe8), UINT32_C(0x80052f5c),
        &REG(run, NBA97_MATCH_INITIALIZE_V0)));
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A0), 700);
    TRY(branch_known(run, &REG(run, NBA97_MATCH_INITIALIZE_V1),
        UINT32_C(0x80052f60), &alternate));
    if (!alternate) {
        TRY(call_nop(run, UINT32_C(0x80052f68),
            UINT32_C(0x8004dc08), 0));
    } else {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 480);
        TRY(call_clear(run, UINT32_C(0x80052f7c),
            UINT32_C(0x80029bcc), 3, NBA97_MATCH_INITIALIZE_A2));
        TRY(call_nop(run, UINT32_C(0x80052f84),
            UINT32_C(0x8004fd38), 0));
        TRY(call_nop(run, UINT32_C(0x80052f8c),
            UINT32_C(0x80029bd4), 0));
    }

    /* 0x80052F94..0x80052FEC synchronizes, then releases the player model,
     * optional normal-only resource, palette, and head buffers in source order. */
    TRY(call_clear(run, UINT32_C(0x80052f94),
        UINT32_C(0x800994f4), 1, NBA97_MATCH_INITIALIZE_A0));
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
        UINT32_C(0x801041a0), UINT32_C(0x80052fa0)));
    TRY(call_nop(run, UINT32_C(0x80052fa4),
        UINT32_C(0x80090698), 1));
    TRY(load_mode_and_branch(run, UINT32_C(0x80052fb0),
        UINT32_C(0x80052fb8), 0, &alternate));
    if (!alternate) {
        TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
            UINT32_C(0x800faba0), UINT32_C(0x80052fc4)));
        TRY(call_nop(run, UINT32_C(0x80052fc8),
            UINT32_C(0x80090698), 1));
    }
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
        UINT32_C(0x800dcbe8), UINT32_C(0x80052fd4)));
    TRY(call_nop(run, UINT32_C(0x80052fd8),
        UINT32_C(0x80090698), 1));
    TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
        UINT32_C(0x800fdb34), UINT32_C(0x80052fe4)));
    TRY(call_nop(run, UINT32_C(0x80052fe8),
        UINT32_C(0x80090698), 1));

    /* 0x80052FF0..0x800530A8 rereads mode before and after cleanup. A child
     * can therefore redirect the later branch into the alternate FAT archive. */
    TRY(load_mode_and_branch(run, UINT32_C(0x80052ff4),
        UINT32_C(0x80052ffc), 1, &alternate));
    if (!alternate) {
        TRY(call_nop(run, UINT32_C(0x80053004),
            UINT32_C(0x8004fd48), 1));
        TRY(call_clear(run, UINT32_C(0x8005300c),
            UINT32_C(0x800994f4), 1, NBA97_MATCH_INITIALIZE_A0));
        TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
            UINT32_C(0x80102918), UINT32_C(0x80053018)));
        TRY(call_nop(run, UINT32_C(0x8005301c),
            UINT32_C(0x80090698), 1));
        TRY(load_fixed(run, NBA97_MATCH_INITIALIZE_A0,
            UINT32_C(0x800f9fc0), UINT32_C(0x80053028)));
        TRY(call_nop(run, UINT32_C(0x8005302c),
            UINT32_C(0x80090698), 1));
        TRY(load_mode_and_branch(run, UINT32_C(0x80053038),
            UINT32_C(0x80053040), 1, &alternate));
        if (!alternate) {
            TRY(call_nop(run, UINT32_C(0x80053048),
                UINT32_C(0x800504a8), 0));
        }
    }
    if (alternate) {
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), 480);
        TRY(call_clear(run, UINT32_C(0x8005305c),
            UINT32_C(0x80029bcc), 3, NBA97_MATCH_INITIALIZE_A2));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x800263f4));
        TRY(call_clear(run, UINT32_C(0x8005306c),
            UINT32_C(0x80029bfc), 2, NBA97_MATCH_INITIALIZE_A1));
        REG(run, GPR_S2) = REG(run, NBA97_MATCH_INITIALIZE_V0);
        REG(run, NBA97_MATCH_INITIALIZE_A0) = REG(run, GPR_S2);
        TRY(store_fixed(run, UINT32_C(0x80100000),
            UINT32_C(0x801063c4), UINT32_C(0x80053080),
            &REG(run, NBA97_MATCH_INITIALIZE_A0)));
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800f0ed8), UINT32_C(0x80053088),
            &REG(run, NBA97_MATCH_INITIALIZE_A0)));
        TRY(store_fixed(run, UINT32_C(0x800f0000),
            UINT32_C(0x800f0ed4), UINT32_C(0x80053090),
            &REG(run, NBA97_MATCH_INITIALIZE_A0)));
        TRY(call_nop(run, UINT32_C(0x80053094),
            UINT32_C(0x80050dd0), 0));
        TRY(call_nop(run, UINT32_C(0x8005309c),
            UINT32_C(0x80050dc8), 0));
        TRY(call_nop(run, UINT32_C(0x800530a4),
            UINT32_C(0x80029bd4), 0));
    }

    /* 0x800530AC..0x800530DC performs the final live-mode gate and publishes
     * the normal-only net archive result. */
    TRY(load_mode_and_branch(run, UINT32_C(0x800530b0),
        UINT32_C(0x800530b8), 0, &alternate));
    if (!alternate) {
        TRY(call_nop(run, UINT32_C(0x800530c0),
            UINT32_C(0x800479b8), 0));
        constant(&REG(run, NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x80026404));
        TRY(call_clear(run, UINT32_C(0x800530d0),
            UINT32_C(0x80029bfc), 2, NBA97_MATCH_INITIALIZE_A1));
        TRY(store_fixed(run, UINT32_C(0x80100000),
            UINT32_C(0x80103f44), UINT32_C(0x800530dc),
            &REG(run, NBA97_MATCH_INITIALIZE_V0)));
    }

    /* 0x800530E0..0x800530F8 reloads through the mutable live sp in the exact
     * ra/s2/s1/s0 order, advances that sp, then requires the JR target. */
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0x800530e0)));
    TRY(read_word(run, REG(run, NBA97_MATCH_INITIALIZE_SP).word + 0x1cu,
        UINT32_C(0x800530e0), &REG(run, NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = REG(run, NBA97_MATCH_INITIALIZE_RA);
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0x800530e4)));
    TRY(read_word(run, REG(run, NBA97_MATCH_INITIALIZE_SP).word + 0x18u,
        UINT32_C(0x800530e4), &REG(run, GPR_S2)));
    out->restored_saved_register[0] = REG(run, GPR_S2);
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0x800530e8)));
    TRY(read_word(run, REG(run, NBA97_MATCH_INITIALIZE_SP).word + 0x14u,
        UINT32_C(0x800530e8), &REG(run, GPR_S1)));
    out->restored_saved_register[1] = REG(run, GPR_S1);
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0x800530ec)));
    TRY(read_word(run, REG(run, NBA97_MATCH_INITIALIZE_SP).word + 0x10u,
        UINT32_C(0x800530ec), &REG(run, NBA97_MATCH_INITIALIZE_S0)));
    out->restored_saved_register[2] = REG(run, NBA97_MATCH_INITIALIZE_S0);
    add_constant(&REG(run, NBA97_MATCH_INITIALIZE_SP),
        &REG(run, NBA97_MATCH_INITIALIZE_SP), 0x20);
    TRY(require_known(run, &REG(run, NBA97_MATCH_INITIALIZE_RA),
        UINT32_C(0x800530f4)));
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
