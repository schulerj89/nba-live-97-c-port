#include "game_period_startup.h"

#include <string.h>

#define PERIOD_SELECTOR_ADDRESS UINT32_C(0x800fdb68)
#define BALL_POINTER_ADDRESS UINT32_C(0x80020c14)
#define START_COUNTER_ADDRESS UINT32_C(0x800fdb92)
#define ACTIVE_BALL_ADDRESS UINT32_C(0x800fdc48)
#define FRAME_DELTA_ADDRESS UINT32_C(0x800fdb6c)
#define OPTIONAL_FLAG_ADDRESS UINT32_C(0x8001edec)

typedef struct Nba97GamePeriodStartupRun {
    Nba97GamePeriodStartupContext* context;
    Nba97GamePeriodStartupProgress* out;
    Nba97GamePeriodStartupRegisters registers;
} Nba97GamePeriodStartupRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GamePeriodStartupRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GamePeriodStartupRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GamePeriodStartupRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
    return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}

static void journal(Nba97GamePeriodStartupRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GamePeriodStartupWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GamePeriodStartupAccess* event =
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

static int locate(Nba97GamePeriodStartupRun* run, uint32_t address,
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

static int read_value(Nba97GamePeriodStartupRun* run, uint32_t address,
    uint8_t width, uint32_t pc, Nba97GamePeriodStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GamePeriodStartupWord loaded;
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
    journal(run, NBA97_GAME_PERIOD_STARTUP_READ, pc, address, width, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GamePeriodStartupRun* run, uint32_t address,
    uint8_t width, uint32_t pc, const Nba97GamePeriodStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GamePeriodStartupWord stored = *value;
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
    journal(run, NBA97_GAME_PERIOD_STARTUP_STORE, pc, address, width, &stored);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(const Nba97GamePeriodStartupRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GamePeriodStartupContext* context,
    Nba97GamePeriodStartupProgress* out,
    Nba97GamePeriodStartupRun* run) {
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

/* Determine byte knowledge after a wrapping 32-bit addition without assuming
 * values for unknown input bytes. The representative word still follows the
 * supplied raw bits, while known bytes are only those invariant for every
 * possible unknown-byte value and carry. */
static Nba97GamePeriodStartupWord add_constant(
    Nba97GamePeriodStartupWord input, uint32_t constant) {
    Nba97GamePeriodStartupWord result;
    unsigned byte;
    unsigned carry_mask = 1u; /* bit0: carry zero is possible */
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

static int effective_address(Nba97GamePeriodStartupRun* run,
    unsigned base_register, uint32_t offset, uint32_t pc,
    uint32_t* address) {
    if (run->registers.gpr[base_register].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = run->registers.gpr[base_register].word + offset;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GamePeriodStartupWord sign_extend_half(
    Nba97GamePeriodStartupWord source) {
    Nba97GamePeriodStartupWord result;
    uint32_t low = source.word & 0xffffu;
    result.word = (low & 0x8000u) ? low | UINT32_C(0xffff0000) : low;
    result.known_mask = (uint8_t)(source.known_mask & 0x03u);
    if (source.known_mask & 0x02u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static Nba97GamePeriodStartupWord zero_extend_half(
    Nba97GamePeriodStartupWord source) {
    Nba97GamePeriodStartupWord result;
    result.word = source.word & 0xffffu;
    result.known_mask =
        (uint8_t)((source.known_mask & 0x03u) | 0x0cu);
    return result;
}

static int decide_zero(Nba97GamePeriodStartupRun* run,
    const Nba97GamePeriodStartupWord* value, uint32_t pc, int* is_zero) {
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

enum Nba97GamePeriodStartupDelayKind {
    NBA97_GAME_PERIOD_STARTUP_DELAY_NOP = 0,
    NBA97_GAME_PERIOD_STARTUP_DELAY_CONSTANT,
    NBA97_GAME_PERIOD_STARTUP_DELAY_SAVE_S0
};

static int invoke(Nba97GamePeriodStartupRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    enum Nba97GamePeriodStartupDelayKind delay_kind,
    unsigned delay_destination, uint32_t delay_value) {
    Nba97GamePeriodStartupEvent event;
    int accepted;
    uint32_t address;
    /* Every callback sees JAL's ra and the completed delay instruction. A
     * refusal or exhausted callback budget cannot roll either effect back. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = pc + 8u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    if (delay_kind == NBA97_GAME_PERIOD_STARTUP_DELAY_CONSTANT) {
        run->registers.gpr[delay_destination].word = delay_value;
        run->registers.gpr[delay_destination].known_mask = 0x0fu;
    } else if (delay_kind == NBA97_GAME_PERIOD_STARTUP_DELAY_SAVE_S0) {
        TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x10u,
            pc + 4u, &address));
        TRY(write_value(run, address, 4, pc + 4u,
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    }
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
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

int nba97_game_period_startup(Nba97GamePeriodStartupContext* context,
    Nba97GamePeriodStartupProgress* out) {
    Nba97GamePeriodStartupRun storage;
    Nba97GamePeriodStartupRun* run = &storage;
    Nba97GamePeriodStartupWord value;
    uint32_t address;
    int is_zero;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80067468..0x8006747C: ADDIU propagates partial sp knowledge.
     * The first JAL assigns ra before its delay-slot save of incoming s0. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_constant(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP], UINT32_C(0xffffffe8));
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x14u,
        UINT32_C(0x8006746c), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8006746c),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    TRY(invoke(run, UINT32_C(0x80067470), UINT32_C(0x80065db0),
        NBA97_GAME_PERIOD_STARTUP_PERIOD_INITIALIZE, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_SAVE_S0, 0, 0));
    TRY(invoke(run, UINT32_C(0x80067478), UINT32_C(0x80063edc),
        NBA97_GAME_PERIOD_STARTUP_PLAYER_ATTRIBUTES, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));

    /* GAMEONLY 0x80067480..0x800674A8: signed LH controls the mutually
     * exclusive child. The nonzero child receives a0=1 from its JAL delay. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        (Nba97GamePeriodStartupWord){UINT32_C(0x80100000), 0x0f};
    TRY(read_value(run, PERIOD_SELECTOR_ADDRESS, 2,
        UINT32_C(0x80067484), &value));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = sign_extend_half(value);
    out->period_selector =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    TRY(decide_zero(run, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
        UINT32_C(0x8006748c), &is_zero));
    if (is_zero) {
        TRY(invoke(run, UINT32_C(0x80067494), UINT32_C(0x800673f0),
            NBA97_GAME_PERIOD_STARTUP_ZERO_PERIOD_SERVICE, 0,
            NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    } else {
        out->used_nonzero_period_path = 1;
        TRY(invoke(run, UINT32_C(0x800674a4), UINT32_C(0x80067194),
            NBA97_GAME_PERIOD_STARTUP_NONZERO_PERIOD_SERVICE, 1,
            NBA97_GAME_PERIOD_STARTUP_DELAY_CONSTANT,
            NBA97_MATCH_INITIALIZE_A0, 1));
    }

    /* GAMEONLY 0x800674AC..0x800674C4: preserve all three JAL delays: s0=1,
     * a1=-1, and a0=15. The explicit a0=1 precedes the 35318 JAL. */
    TRY(invoke(run, UINT32_C(0x800674ac), UINT32_C(0x8002a25c),
        NBA97_GAME_PERIOD_STARTUP_A25C, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_S0, 1));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GamePeriodStartupWord){1, 0x0f};
    TRY(invoke(run, UINT32_C(0x800674b8), UINT32_C(0x80035318),
        NBA97_GAME_PERIOD_STARTUP_35318, 2,
        NBA97_GAME_PERIOD_STARTUP_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A1, UINT32_MAX));
    TRY(invoke(run, UINT32_C(0x800674c0), UINT32_C(0x80029590),
        NBA97_GAME_PERIOD_STARTUP_29590, 1,
        NBA97_GAME_PERIOD_STARTUP_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A0, 15));

    /* GAMEONLY 0x800674C8..0x800674EC: publish the live pointer only after
     * the first live-s0 halfword; the frame pump may then replace s0 before
     * the second halfword is written. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        (Nba97GamePeriodStartupWord){UINT32_C(0x80020000), 0x0f};
    TRY(read_value(run, BALL_POINTER_ADDRESS, 4,
        UINT32_C(0x800674cc),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    out->published_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
        (Nba97GamePeriodStartupWord){UINT32_C(0x80100000), 0x0f};
    TRY(write_value(run, START_COUNTER_ADDRESS, 2,
        UINT32_C(0x800674d4),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
        (Nba97GamePeriodStartupWord){UINT32_C(0x80100000), 0x0f};
    TRY(write_value(run, ACTIVE_BALL_ADDRESS, 4,
        UINT32_C(0x800674dc),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    TRY(invoke(run, UINT32_C(0x800674e0), UINT32_C(0x8002dd84),
        NBA97_GAME_PERIOD_STARTUP_FRAME_PUMP, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
        (Nba97GamePeriodStartupWord){UINT32_C(0x80100000), 0x0f};
    TRY(write_value(run, FRAME_DELTA_ADDRESS, 2,
        UINT32_C(0x800674ec),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));

    /* GAMEONLY 0x800674F0..0x8006751C: repeated calls are separate source
     * boundaries; each receives the complete state returned by the prior one. */
    TRY(invoke(run, UINT32_C(0x800674f0), UINT32_C(0x80076b28),
        NBA97_GAME_PERIOD_STARTUP_76B28, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    TRY(invoke(run, UINT32_C(0x800674f8), UINT32_C(0x80076b3c),
        NBA97_GAME_PERIOD_STARTUP_76B3C, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    TRY(invoke(run, UINT32_C(0x80067500), UINT32_C(0x80076b28),
        NBA97_GAME_PERIOD_STARTUP_76B28, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    TRY(invoke(run, UINT32_C(0x80067508), UINT32_C(0x80076b3c),
        NBA97_GAME_PERIOD_STARTUP_76B3C, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    TRY(invoke(run, UINT32_C(0x80067510), UINT32_C(0x800a584c),
        NBA97_GAME_PERIOD_STARTUP_A584C, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    TRY(invoke(run, UINT32_C(0x80067518), UINT32_C(0x800a584c),
        NBA97_GAME_PERIOD_STARTUP_A584C, 0,
        NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));

    /* GAMEONLY 0x80067520..0x80067538: LHU zero-extends even partially known
     * bytes. Any known nonzero byte resolves the optional branch immediately. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        (Nba97GamePeriodStartupWord){UINT32_C(0x80020000), 0x0f};
    TRY(read_value(run, OPTIONAL_FLAG_ADDRESS, 2,
        UINT32_C(0x80067524), &value));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = zero_extend_half(value);
    out->optional_flag =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    TRY(decide_zero(run, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
        UINT32_C(0x8006752c), &is_zero));
    if (!is_zero) {
        out->optional_service_called = 1;
        TRY(invoke(run, UINT32_C(0x80067534), UINT32_C(0x80035678),
            NBA97_GAME_PERIOD_STARTUP_35678, 0,
            NBA97_GAME_PERIOD_STARTUP_DELAY_NOP, 0, 0));
    }

    /* GAMEONLY 0x8006753C..0x8006754C: both loads use the child-mutable sp.
     * ADDIU still executes after an unknown ra load; only JR consumes ra. */
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x14u,
        UINT32_C(0x8006753c), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x8006753c),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x10u,
        UINT32_C(0x80067540), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x80067540),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    out->restored_s0 = run->registers.gpr[NBA97_MATCH_INITIALIZE_S0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_constant(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP], 0x18u);
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80067548), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
