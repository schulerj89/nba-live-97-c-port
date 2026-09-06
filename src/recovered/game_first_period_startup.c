#include "game_first_period_startup.h"

#include <string.h>

#define PRESENTATION_FLAG_ADDRESS UINT32_C(0x800eb680)
#define PRESENTATION_CLEAR_ADDRESS UINT32_C(0x800fdb4e)
#define FIRST_PERIOD_MARKER_ADDRESS UINT32_C(0x800fdb94)

typedef struct Nba97GameFirstPeriodStartupRun {
    Nba97GameFirstPeriodStartupContext* context;
    Nba97GameFirstPeriodStartupProgress* out;
    Nba97GameFirstPeriodStartupRegisters registers;
} Nba97GameFirstPeriodStartupRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameFirstPeriodStartupRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameFirstPeriodStartupRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameFirstPeriodStartupRun* run) {
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

static void journal(Nba97GameFirstPeriodStartupRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameFirstPeriodStartupWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameFirstPeriodStartupAccess* event =
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

static int locate(Nba97GameFirstPeriodStartupRun* run, uint32_t address,
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

static int read_value(Nba97GameFirstPeriodStartupRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    Nba97GameFirstPeriodStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameFirstPeriodStartupWord loaded;
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
    journal(run, NBA97_GAME_FIRST_PERIOD_STARTUP_READ, pc, address, width,
        value);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameFirstPeriodStartupRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    const Nba97GameFirstPeriodStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameFirstPeriodStartupWord stored = *value;
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
    journal(run, NBA97_GAME_FIRST_PERIOD_STARTUP_STORE, pc, address, width,
        &stored);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(
    const Nba97GameFirstPeriodStartupRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameFirstPeriodStartupContext* context,
    Nba97GameFirstPeriodStartupProgress* out,
    Nba97GameFirstPeriodStartupRun* run) {
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

/* Propagate per-byte knowledge through wrapping 32-bit ADDIU without choosing
 * values for unknown source bytes or possible carries. */
static Nba97GameFirstPeriodStartupWord add_constant(
    Nba97GameFirstPeriodStartupWord input, uint32_t constant) {
    Nba97GameFirstPeriodStartupWord result;
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

static int effective_address(Nba97GameFirstPeriodStartupRun* run,
    unsigned base_register, uint32_t offset, uint32_t pc,
    uint32_t* address) {
    if (run->registers.gpr[base_register].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = run->registers.gpr[base_register].word + offset;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameFirstPeriodStartupWord zero_extend_byte(
    Nba97GameFirstPeriodStartupWord source) {
    Nba97GameFirstPeriodStartupWord result;
    result.word = source.word & 0xffu;
    result.known_mask = (uint8_t)((source.known_mask & 0x01u) | 0x0eu);
    return result;
}

static int decide_zero(Nba97GameFirstPeriodStartupRun* run,
    const Nba97GameFirstPeriodStartupWord* value, uint32_t pc,
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

enum Nba97GameFirstPeriodStartupDelayKind {
    NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP = 0,
    NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_A0_ONE
};

static int invoke(Nba97GameFirstPeriodStartupRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    enum Nba97GameFirstPeriodStartupDelayKind delay_kind) {
    Nba97GameFirstPeriodStartupEvent event;
    int accepted;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = pc + 8u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    if (delay_kind == NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_A0_ONE) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word = 1;
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 0x0fu;
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

int nba97_game_first_period_startup(
    Nba97GameFirstPeriodStartupContext* context,
    Nba97GameFirstPeriodStartupProgress* out) {
    Nba97GameFirstPeriodStartupRun storage;
    Nba97GameFirstPeriodStartupRun* run = &storage;
    Nba97GameFirstPeriodStartupWord value;
    uint32_t address;
    int is_zero;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800673F0..0x80067404: create the 0x18-byte frame, save ra,
     * and run the first two children with explicit NOP delay slots. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_constant(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP], UINT32_C(0xffffffe8));
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x10u,
        UINT32_C(0x800673f4), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x800673f4),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    TRY(invoke(run, UINT32_C(0x800673f8), UINT32_C(0x800295d0),
        NBA97_GAME_FIRST_PERIOD_STARTUP_295D0, 0,
        NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP));
    TRY(invoke(run, UINT32_C(0x80067400), UINT32_C(0x8002a244),
        NBA97_GAME_FIRST_PERIOD_STARTUP_2A244, 0,
        NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP));

    /* GAMEONLY 0x80067408..0x80067430: LBU makes the three upper bytes of v0
     * known zero. The branch NOP executes before an unknown byte stops here. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        (Nba97GameFirstPeriodStartupWord){UINT32_C(0x800f0000), 0x0f};
    TRY(read_value(run, PRESENTATION_FLAG_ADDRESS, 1,
        UINT32_C(0x8006740c), &value));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = zero_extend_byte(value);
    out->presentation_flag =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    TRY(decide_zero(run, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
        UINT32_C(0x80067414), &is_zero));
    if (!is_zero) {
        out->optional_presentation_executed = 1;
        TRY(invoke(run, UINT32_C(0x8006741c), UINT32_C(0x8002dd84),
            NBA97_GAME_FIRST_PERIOD_STARTUP_FRAME_PUMP, 0,
            NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP));
        TRY(invoke(run, UINT32_C(0x80067424), UINT32_C(0x8002ddcc),
            NBA97_GAME_FIRST_PERIOD_STARTUP_2DDCC, 0,
            NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
            (Nba97GameFirstPeriodStartupWord){UINT32_C(0x80100000), 0x0f};
        value = (Nba97GameFirstPeriodStartupWord){0, 0x0f};
        TRY(write_value(run, PRESENTATION_CLEAR_ADDRESS, 2,
            UINT32_C(0x80067430), &value));
    }

    /* GAMEONLY 0x80067434..0x80067444: JAL assigns ra before its delay slot
     * forces a0=1; the following instructions publish the signed -1 marker. */
    TRY(invoke(run, UINT32_C(0x80067434), UINT32_C(0x8002a254),
        NBA97_GAME_FIRST_PERIOD_STARTUP_2A254, 1,
        NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_A0_ONE));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        (Nba97GameFirstPeriodStartupWord){UINT32_MAX, 0x0f};
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
        (Nba97GameFirstPeriodStartupWord){UINT32_C(0x80100000), 0x0f};
    TRY(write_value(run, FIRST_PERIOD_MARKER_ADDRESS, 2,
        UINT32_C(0x80067444),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));

    /* GAMEONLY 0x80067448..0x80067454: each typed child sees the complete
     * live result of its predecessor and retains its own NOP delay slot. */
    TRY(invoke(run, UINT32_C(0x80067448), UINT32_C(0x80065db0),
        NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0, 0,
        NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP));
    TRY(invoke(run, UINT32_C(0x80067450), UINT32_C(0x8007ef4c),
        NBA97_GAME_FIRST_PERIOD_STARTUP_7EF4C, 0,
        NBA97_GAME_FIRST_PERIOD_STARTUP_DELAY_NOP));

    /* GAMEONLY 0x80067458..0x80067464: reload ra through child-mutable sp,
     * then execute ADDIU before JR consumes a possibly unknown return value. */
    TRY(effective_address(run, NBA97_MATCH_INITIALIZE_SP, 0x10u,
        UINT32_C(0x80067458), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x80067458),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_constant(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP], 0x18u);
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80067460), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
