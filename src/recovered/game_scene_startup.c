#include "game_scene_startup.h"

#include <string.h>

typedef struct Nba97GameSceneStartupRun {
    Nba97GameSceneStartupContext* context;
    Nba97GameSceneStartupProgress* out;
    Nba97GameSceneStartupRegisters registers;
} Nba97GameSceneStartupRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameSceneStartupRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameSceneStartupRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameSceneStartupWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static Nba97GameSceneStartupWord add_words(
    Nba97GameSceneStartupWord left, Nba97GameSceneStartupWord right) {
    Nba97GameSceneStartupWord result;
    unsigned i;
    int lower_known = 1;
    result.word = left.word + right.word;
    result.known_mask = 0;
    for (i = 0; i < 4; ++i) {
        lower_known = lower_known &&
            (left.known_mask & (1u << i)) &&
            (right.known_mask & (1u << i));
        if (lower_known)
            result.known_mask = (uint8_t)(result.known_mask | (1u << i));
    }
    return result;
}

static Nba97GameSceneStartupWord subtract_words(
    Nba97GameSceneStartupWord left, Nba97GameSceneStartupWord right) {
    Nba97GameSceneStartupWord result;
    unsigned i;
    int lower_known = 1;
    result.word = left.word - right.word;
    result.known_mask = 0;
    for (i = 0; i < 4; ++i) {
        lower_known = lower_known &&
            (left.known_mask & (1u << i)) &&
            (right.known_mask & (1u << i));
        if (lower_known)
            result.known_mask = (uint8_t)(result.known_mask | (1u << i));
    }
    return result;
}

static Nba97GameSceneStartupWord shift_left(
    Nba97GameSceneStartupWord source, unsigned amount) {
    Nba97GameSceneStartupWord result;
    uint32_t known_bits = 0;
    uint32_t shifted_known;
    unsigned i;
    for (i = 0; i < 4; ++i)
        if (source.known_mask & (1u << i))
            known_bits |= UINT32_C(0xff) << (8u * i);
    result.word = source.word << amount;
    shifted_known = (known_bits << amount) | ((UINT32_C(1) << amount) - 1u);
    result.known_mask = 0;
    for (i = 0; i < 4; ++i)
        if (((shifted_known >> (8u * i)) & 0xffu) == 0xffu)
            result.known_mask = (uint8_t)(result.known_mask | (1u << i));
    return result;
}

static Nba97GameSceneStartupWord predicate_word(uint32_t result,
    const Nba97GameSceneStartupWord* source) {
    Nba97GameSceneStartupWord value;
    value.word = result;
    /* A comparison with unknown input still proves the upper three zero
     * bytes of its 0/1 result. The low byte remains unresolved. */
    value.known_mask = source->known_mask == 0x0fu ? 0x0fu : 0x0eu;
    return value;
}

static int equality_known(const Nba97GameSceneStartupWord* left,
    const Nba97GameSceneStartupWord* right, int* equal) {
    unsigned i;
    for (i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((left->known_mask & bit) && (right->known_mask & bit) &&
            ((left->word >> (8u * i)) & 0xffu) !=
            ((right->word >> (8u * i)) & 0xffu)) {
            *equal = 0;
            return 1;
        }
    }
    if (left->known_mask == 0x0fu && right->known_mask == 0x0fu) {
        *equal = left->word == right->word;
        return 1;
    }
    return 0;
}

static int spend(Nba97GameSceneStartupRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int require_known(Nba97GameSceneStartupRun* run,
    const Nba97GameSceneStartupWord* value, uint32_t pc,
    uint32_t address, uint32_t entry) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, address, entry);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameSceneStartupRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width, uint32_t word,
    uint8_t known_mask) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameSceneStartupAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = word;
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask = known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameSceneStartupRun* run, uint32_t address,
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

static int read_word(Nba97GameSceneStartupRun* run, uint32_t address,
    uint32_t pc, Nba97GameSceneStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    value->word = 0;
    value->known_mask = 0;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        value->word |= (uint32_t)data[i] << (8u * i);
        if (!known || known[i])
            value->known_mask = (uint8_t)(value->known_mask | (1u << i));
    }
    ++run->out->reads;
    journal(run, NBA97_GAME_SCENE_STARTUP_READ, pc, address, 4,
        value->word, value->known_mask);
    return NBA97_TEXT_COMPLETE;
}

static int read_half(Nba97GameSceneStartupRun* run, uint32_t address,
    uint32_t pc, Nba97GameSceneStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    uint16_t half;
    uint8_t source_mask = 0;
    TRY(locate(run, address, 2, 2, pc, &data, &known));
    half = (uint16_t)((uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8));
    if (!known || known[0])
        source_mask |= 1u;
    if (!known || known[1])
        source_mask |= 2u;
    value->word = (uint32_t)half;
    if (half & UINT16_C(0x8000))
        value->word |= UINT32_C(0xffff0000);
    value->known_mask = (uint8_t)((source_mask & 1u) |
        ((source_mask & 2u) ? 0x0eu : 0u));
    ++run->out->reads;
    journal(run, NBA97_GAME_SCENE_STARTUP_READ, pc, address, 2,
        value->word, source_mask);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameSceneStartupRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    const Nba97GameSceneStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    uint8_t width_mask = (uint8_t)((1u << width) - 1u);
    unsigned i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    if (!known && (value->known_mask & width_mask) != width_mask)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value->word >> (8u * i));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_SCENE_STARTUP_STORE, pc, address,
        (uint8_t)width, value->word,
        (uint8_t)(value->known_mask & width_mask));
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameSceneStartupRun* run, uint32_t address,
    uint32_t pc, const Nba97GameSceneStartupWord* value) {
    return write_value(run, address, 4, 4, pc, value);
}

static int write_half(Nba97GameSceneStartupRun* run, uint32_t address,
    uint32_t pc, const Nba97GameSceneStartupWord* value) {
    return write_value(run, address, 2, 2, pc, value);
}

static int write_known_word(Nba97GameSceneStartupRun* run,
    uint32_t address, uint32_t pc, uint32_t word) {
    Nba97GameSceneStartupWord value;
    set_known(&value, word);
    return write_word(run, address, pc, &value);
}

static int write_known_half(Nba97GameSceneStartupRun* run,
    uint32_t address, uint32_t pc, uint16_t half) {
    Nba97GameSceneStartupWord value;
    set_known(&value, half);
    return write_half(run, address, pc, &value);
}

static int registers_valid(const Nba97GameSceneStartupRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameSceneStartupContext* context,
    Nba97GameSceneStartupProgress* out, Nba97GameSceneStartupRun* run) {
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

static int dynamic_word_read(Nba97GameSceneStartupRun* run,
    const Nba97GameSceneStartupWord* address, uint32_t pc,
    Nba97GameSceneStartupWord* value) {
    TRY(require_known(run, address, pc, address->word, 0));
    return read_word(run, address->word, pc, value);
}

static int dynamic_half_read(Nba97GameSceneStartupRun* run,
    const Nba97GameSceneStartupWord* address, uint32_t pc,
    Nba97GameSceneStartupWord* value) {
    TRY(require_known(run, address, pc, address->word, 0));
    return read_half(run, address->word, pc, value);
}

static int dynamic_word_write(Nba97GameSceneStartupRun* run,
    const Nba97GameSceneStartupWord* address, uint32_t pc,
    const Nba97GameSceneStartupWord* value) {
    TRY(require_known(run, address, pc, address->word, 0));
    return write_word(run, address->word, pc, value);
}

static void jal(Nba97GameSceneStartupRun* run, uint32_t pc) {
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
}

static int invoke(Nba97GameSceneStartupRun* run, uint32_t pc,
    uint32_t delay_slot_pc, uint32_t entry, uint8_t kind,
    uint8_t argument_count) {
    Nba97GameSceneStartupEvent event;
    int accepted;
    stop(run, pc, 0, entry);
    TRY(spend(run));
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
    accepted = run->context->io(run->context->user,
        &run->context->memory, &event, &run->registers);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!registers_valid(&run->registers))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int nop_call(Nba97GameSceneStartupRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count) {
    jal(run, pc);
    return invoke(run, pc, pc + 4u, entry, kind, argument_count);
}

static int add_immediate(Nba97GameSceneStartupRun* run,
    Nba97GameSceneStartupWord* value, uint32_t amount, uint32_t pc) {
    Nba97GameSceneStartupWord immediate;
    (void)run;
    (void)pc;
    set_known(&immediate, amount);
    *value = add_words(*value, immediate);
    return NBA97_TEXT_COMPLETE;
}

static uint32_t signed_less_than_positive(uint32_t word, uint32_t limit) {
    return (word & UINT32_C(0x80000000)) != 0 || word < limit;
}

static int selector_address(Nba97GameSceneStartupRun* run,
    Nba97GameSceneStartupWord* selector, Nba97GameSceneStartupWord* result,
    uint32_t first_pc, int display) {
    Nba97GameSceneStartupWord word;
    (void)run;
    (void)first_pc;
    word = *selector;
    if (display) {
        /* SLL, ADDU, SLL at 0x80048F0C..0x80048F14 / F64..F6C. */
        word = shift_left(word, 2);
        word = add_words(word, *selector);
        word = shift_left(word, 2);
    } else {
        /* 0x80048F38..0x80048F48 / F8C..F9C forms 0x5c * selector. */
        word = shift_left(word, 1);
        word = add_words(word, *selector);
        word = shift_left(word, 3);
        word = subtract_words(word, *selector);
        word = shift_left(word, 2);
    }
    *result = word;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_scene_startup(Nba97GameSceneStartupContext* context,
    Nba97GameSceneStartupProgress* out) {
    Nba97GameSceneStartupRun storage;
    Nba97GameSceneStartupRun* run = &storage;
    Nba97GameSceneStartupWord pending;
    Nba97GameSceneStartupWord value;
    unsigned keep_running;
    int branch_known;
    int equal;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80048D5C..0x80048DA4: build the wrapping 0x28-byte frame,
     * save live callee-saved GPRs in source order, then reset four globals. */
    TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0xffffffd8), UINT32_C(0x80048d5c)));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x10u;
    TRY(dynamic_word_write(run, &value, UINT32_C(0x80048d60),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    /* The five frame offsets are applied explicitly to the live frame base. */
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), 0);
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x1cu;
    TRY(dynamic_word_write(run, &value, UINT32_C(0x80048d68),
        &R(NBA97_MATCH_INITIALIZE_S0 + 3)));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0 + 3), UINT32_C(0x3e1a));
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x18u;
    TRY(dynamic_word_write(run, &value, UINT32_C(0x80048d70),
        &R(NBA97_MATCH_INITIALIZE_S0 + 2)));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0 + 2), 2);
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x14u;
    TRY(dynamic_word_write(run, &value, UINT32_C(0x80048d78),
        &R(NBA97_MATCH_INITIALIZE_S0 + 1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0 + 1), UINT32_C(0x800faba4));
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x20u;
    TRY(dynamic_word_write(run, &value, UINT32_C(0x80048d84),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800f0000));
    TRY(write_known_word(run, UINT32_C(0x800eb684),
        UINT32_C(0x80048d8c), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x800fa374),
        UINT32_C(0x80048d94), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x80104248),
        UINT32_C(0x80048d9c), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x800febf4),
        UINT32_C(0x80048da4), 0));

    /* 0x80048DA8..0x80048DCC: each controller slot is cleared before its
     * query. Child-mutated s0/s1/s2/s3 remain live through compare and delay. */
    do {
        TRY(dynamic_word_write(run, &R(NBA97_MATCH_INITIALIZE_S0 + 1),
            UINT32_C(0x80048da8), &R(NBA97_MATCH_INITIALIZE_ZERO)));
        jal(run, UINT32_C(0x80048dac));
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
        TRY(invoke(run, UINT32_C(0x80048dac), UINT32_C(0x80048db0),
            UINT32_C(0x8008f224),
            NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224, 1));
        branch_known = equality_known(&R(NBA97_MATCH_INITIALIZE_V0),
            &R(NBA97_MATCH_INITIALIZE_S0 + 3), &equal);
        if (!branch_known) {
            stop(run, UINT32_C(0x80048db4), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (equal) {
            TRY(dynamic_word_write(run, &R(NBA97_MATCH_INITIALIZE_S0 + 1),
                UINT32_C(0x80048dbc), &R(NBA97_MATCH_INITIALIZE_S0 + 2)));
            ++out->controller_matches;
        }
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_S0), 1,
            UINT32_C(0x80048dc0)));
        R(NBA97_MATCH_INITIALIZE_V0) = predicate_word(
            signed_less_than_positive(R(NBA97_MATCH_INITIALIZE_S0).word, 8),
            &R(NBA97_MATCH_INITIALIZE_S0));
        branch_known = equality_known(&R(NBA97_MATCH_INITIALIZE_V0),
            &R(NBA97_MATCH_INITIALIZE_ZERO), &equal);
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_S0 + 1), 4,
            UINT32_C(0x80048dcc)));
        ++out->controller_iterations;
        if (!branch_known) {
            stop(run, UINT32_C(0x80048dc8), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        keep_running = !equal;
    } while (keep_running);

    /* 0x80048DD0..0x80048DF4: publish the fixed scene root and invoke the
     * roster preparer after its JAL delay slot clears live s0. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800b8280));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800f0000));
    TRY(write_known_word(run, UINT32_C(0x800eb678),
        UINT32_C(0x80048ddc), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x800fa62c),
        UINT32_C(0x80048de4), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80110000));
    TRY(write_word(run, UINT32_C(0x80109a90), UINT32_C(0x80048dec),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    jal(run, UINT32_C(0x80048df0));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), 0);
    TRY(invoke(run, UINT32_C(0x80048df0), UINT32_C(0x80048df4),
        UINT32_C(0x8004d38c), NBA97_GAME_SCENE_STARTUP_CHILD_8004D38C, 0));

    /* 0x80048DF8..0x80048E4C: do-while copy of signed home/away IDs. The
     * second pointer load overlaps a0's increment; the second LH overlaps
     * live s0's increment exactly as on the R3000 load-delay pipeline. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x8010424c));
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x8010427c));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020b8c));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020bbc));
    R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_V0);
    do {
        TRY(dynamic_word_read(run, &R(NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x80048e14), &pending));
        R(NBA97_MATCH_INITIALIZE_V0) = pending; /* NOP load delay 0x80048E18. */
        TRY(dynamic_half_read(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80048e1c), &pending));
        R(NBA97_MATCH_INITIALIZE_V0) = pending; /* NOP load delay 0x80048E20. */
        TRY(dynamic_word_write(run, &R(NBA97_MATCH_INITIALIZE_V1),
            UINT32_C(0x80048e24), &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(dynamic_word_read(run, &R(NBA97_MATCH_INITIALIZE_A1),
            UINT32_C(0x80048e28), &pending));
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_A0), 4,
            UINT32_C(0x80048e2c)));
        R(NBA97_MATCH_INITIALIZE_V0) = pending;
        TRY(dynamic_half_read(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80048e30), &pending));
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_S0), 1,
            UINT32_C(0x80048e34)));
        R(NBA97_MATCH_INITIALIZE_V0) = pending;
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_A1), 4,
            UINT32_C(0x80048e38)));
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_V1), 4,
            UINT32_C(0x80048e3c)));
        TRY(dynamic_word_write(run, &R(NBA97_MATCH_INITIALIZE_A2),
            UINT32_C(0x80048e40), &R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_V0) = predicate_word(
            signed_less_than_positive(R(NBA97_MATCH_INITIALIZE_S0).word, 12),
            &R(NBA97_MATCH_INITIALIZE_S0));
        branch_known = equality_known(&R(NBA97_MATCH_INITIALIZE_V0),
            &R(NBA97_MATCH_INITIALIZE_ZERO), &equal);
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_A2), 4,
            UINT32_C(0x80048e4c)));
        ++out->roster_iterations;
        if (!branch_known) {
            stop(run, UINT32_C(0x80048e48), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        keep_running = !equal;
    } while (keep_running);

    /* 0x80048E50..0x80048E90: all ten active entities re-read the live
     * 0x800FC650 table pointer before following entity+0x20 to a signed ID. */
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), 0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800fee90));
    do {
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
        TRY(read_word(run, UINT32_C(0x800fc650), UINT32_C(0x80048e60),
            &pending));
        R(NBA97_MATCH_INITIALIZE_V0) = shift_left(
            R(NBA97_MATCH_INITIALIZE_S0), 2);
        R(NBA97_MATCH_INITIALIZE_V1) = pending;
        R(NBA97_MATCH_INITIALIZE_V0) = add_words(
            R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
        TRY(dynamic_word_read(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80048e6c), &pending));
        R(NBA97_MATCH_INITIALIZE_V0) = pending; /* NOP at 0x80048E70. */
        value = R(NBA97_MATCH_INITIALIZE_V0);
        value.word += 0x20u;
        TRY(dynamic_word_read(run, &value, UINT32_C(0x80048e74), &pending));
        R(NBA97_MATCH_INITIALIZE_V0) = pending; /* NOP at 0x80048E78. */
        TRY(dynamic_half_read(run, &R(NBA97_MATCH_INITIALIZE_V0),
            UINT32_C(0x80048e7c), &pending));
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_S0), 1,
            UINT32_C(0x80048e80)));
        R(NBA97_MATCH_INITIALIZE_V0) = pending;
        TRY(dynamic_word_write(run, &R(NBA97_MATCH_INITIALIZE_A0),
            UINT32_C(0x80048e84), &R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_V0) = predicate_word(
            signed_less_than_positive(R(NBA97_MATCH_INITIALIZE_S0).word, 10),
            &R(NBA97_MATCH_INITIALIZE_S0));
        branch_known = equality_known(&R(NBA97_MATCH_INITIALIZE_V0),
            &R(NBA97_MATCH_INITIALIZE_ZERO), &equal);
        TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_A0), 4,
            UINT32_C(0x80048e90)));
        ++out->entity_iterations;
        if (!branch_known) {
            stop(run, UINT32_C(0x80048e8c), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        keep_running = !equal;
    } while (keep_running);

    /* 0x80048E94..0x80048EBC: four resource/render startup services. */
    TRY(nop_call(run, UINT32_C(0x80048e94), UINT32_C(0x80052c20),
        NBA97_GAME_SCENE_STARTUP_CHILD_80052C20, 0));
    TRY(nop_call(run, UINT32_C(0x80048e9c), UINT32_C(0x800a7738),
        NBA97_GAME_SCENE_STARTUP_CHILD_800A7738, 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800b0000));
    TRY(read_word(run, UINT32_C(0x800b729c), UINT32_C(0x80048ea8),
        &pending));
    R(NBA97_MATCH_INITIALIZE_A0) = pending;
    TRY(nop_call(run, UINT32_C(0x80048eac), UINT32_C(0x80056074),
        NBA97_GAME_SCENE_STARTUP_CHILD_80056074, 1));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x100));
    jal(run, UINT32_C(0x80048eb8));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x78));
    TRY(invoke(run, UINT32_C(0x80048eb8), UINT32_C(0x80048ebc),
        UINT32_C(0x8005605c), NBA97_GAME_SCENE_STARTUP_CHILD_8005605C, 2));

    /* 0x80048EC0..0x80048F04: load the first selector before writing camera
     * halfwords in their non-address order; +0x36 is deliberately untouched. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
    TRY(read_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80048ec4), &pending));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x2e00));
    R(NBA97_MATCH_INITIALIZE_V1) = pending;
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_half(run, UINT32_C(0x800fa634), UINT32_C(0x80048ed0),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xfffff95c));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0 + 1), UINT32_C(0x8002205c));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_half(run, UINT32_C(0x800fa630),
        UINT32_C(0x80048ee4), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_half(run, UINT32_C(0x800fa632),
        UINT32_C(0x80048eec), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_half(run, UINT32_C(0x800fa638), UINT32_C(0x80048ef4),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_half(run, UINT32_C(0x800fa63a),
        UINT32_C(0x80048efc), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_half(run, UINT32_C(0x800fa63c),
        UINT32_C(0x80048f04), 0));

    /* 0x80048F08..0x80048FA4: invert and publish the selector twice. Every
     * buffer index is reloaded, while callback-mutated s1/s0 remain live. */
    R(NBA97_MATCH_INITIALIZE_V1) = predicate_word(
        R(NBA97_MATCH_INITIALIZE_V1).word == 0,
        &R(NBA97_MATCH_INITIALIZE_V1));
    TRY(selector_address(run, &R(NBA97_MATCH_INITIALIZE_V1),
        &R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80048f0c), 1));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80048f1c),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    jal(run, UINT32_C(0x80048f20));
    R(NBA97_MATCH_INITIALIZE_A0) = add_words(
        R(NBA97_MATCH_INITIALIZE_A0),
        R(NBA97_MATCH_INITIALIZE_S0 + 1));
    TRY(invoke(run, UINT32_C(0x80048f20), UINT32_C(0x80048f24),
        UINT32_C(0x80099ca4), NBA97_GAME_SCENE_STARTUP_DISPLAY_80099CA4, 1));

    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80048f2c), &pending));
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80020000));
    R(NBA97_MATCH_INITIALIZE_V0) = pending;
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80021eec));
    TRY(selector_address(run, &R(NBA97_MATCH_INITIALIZE_V0),
        &R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80048f38), 0));
    jal(run, UINT32_C(0x80048f4c));
    R(NBA97_MATCH_INITIALIZE_A0) = add_words(
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_S0));
    TRY(invoke(run, UINT32_C(0x80048f4c), UINT32_C(0x80048f50),
        UINT32_C(0x80099acc), NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC, 1));

    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80048f58), &pending));
    R(NBA97_MATCH_INITIALIZE_V0) = pending; /* NOP load delay 0x80048F5C. */
    R(NBA97_MATCH_INITIALIZE_V0) = predicate_word(
        R(NBA97_MATCH_INITIALIZE_V0).word == 0,
        &R(NBA97_MATCH_INITIALIZE_V0));
    TRY(selector_address(run, &R(NBA97_MATCH_INITIALIZE_V0),
        &R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80048f64), 1));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80048f74),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    jal(run, UINT32_C(0x80048f78));
    R(NBA97_MATCH_INITIALIZE_A0) = add_words(
        R(NBA97_MATCH_INITIALIZE_A0),
        R(NBA97_MATCH_INITIALIZE_S0 + 1));
    TRY(invoke(run, UINT32_C(0x80048f78), UINT32_C(0x80048f7c),
        UINT32_C(0x80099ca4), NBA97_GAME_SCENE_STARTUP_DISPLAY_80099CA4, 1));

    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
    TRY(read_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80048f84), &pending));
    R(NBA97_MATCH_INITIALIZE_V0) = pending; /* NOP load delay 0x80048F88. */
    TRY(selector_address(run, &R(NBA97_MATCH_INITIALIZE_V0),
        &R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80048f8c), 0));
    jal(run, UINT32_C(0x80048fa0));
    R(NBA97_MATCH_INITIALIZE_A0) = add_words(
        R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_S0));
    TRY(invoke(run, UINT32_C(0x80048fa0), UINT32_C(0x80048fa4),
        UINT32_C(0x80099acc), NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC, 1));

    /* 0x80048FA8..0x80048FC0: enable rendering, then retain both final
     * child boundaries and every GPR mutation they return. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_half(run, UINT32_C(0x80021498), UINT32_C(0x80048fb0),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(nop_call(run, UINT32_C(0x80048fb4), UINT32_C(0x80063edc),
        NBA97_GAME_SCENE_STARTUP_ATTRIBUTES_80063EDC, 0));
    TRY(nop_call(run, UINT32_C(0x80048fbc), UINT32_C(0x80056944),
        NBA97_GAME_SCENE_STARTUP_CHILD_80056944, 0));

    /* 0x80048FC4..0x80048FE0: the live child-mutated sp controls all five
     * pipelined reloads. Each prior load becomes visible at the next load. */
    TRY(require_known(run, &R(NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0x80048fc4), 0, 0));
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x20u;
    TRY(dynamic_word_read(run, &value, UINT32_C(0x80048fc4), &pending));
    R(NBA97_MATCH_INITIALIZE_RA) = pending;
    out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x1cu;
    TRY(dynamic_word_read(run, &value, UINT32_C(0x80048fc8), &pending));
    R(NBA97_MATCH_INITIALIZE_S0 + 3) = pending;
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x18u;
    TRY(dynamic_word_read(run, &value, UINT32_C(0x80048fcc), &pending));
    R(NBA97_MATCH_INITIALIZE_S0 + 2) = pending;
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x14u;
    TRY(dynamic_word_read(run, &value, UINT32_C(0x80048fd0), &pending));
    R(NBA97_MATCH_INITIALIZE_S0 + 1) = pending;
    value = R(NBA97_MATCH_INITIALIZE_SP);
    value.word += 0x10u;
    TRY(dynamic_word_read(run, &value, UINT32_C(0x80048fd4), &pending));
    R(NBA97_MATCH_INITIALIZE_S0) = pending;
    TRY(add_immediate(run, &R(NBA97_MATCH_INITIALIZE_SP), 0x28u,
        UINT32_C(0x80048fd8)));
    TRY(require_known(run, &R(NBA97_MATCH_INITIALIZE_RA),
        UINT32_C(0x80048fdc), 0, 0));
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
