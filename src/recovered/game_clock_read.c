#include "game_clock_read.h"

#include <string.h>

#define CLOCK_ADDRESS UINT32_C(0x800d7a70)
#define CLOCK_LW_PC UINT32_C(0x800a5814)
#define CLOCK_JR_PC UINT32_C(0x800a5818)

typedef struct Nba97GameClockReadRun {
    Nba97GameClockReadContext* context;
    Nba97GameClockReadProgress* out;
    Nba97GameClockReadMachine machine;
} Nba97GameClockReadRun;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameClockReadRun* run) {
    run->out->machine = run->machine;
    run->out->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
}

static void stop(Nba97GameClockReadRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    publish(run);
}

static void set_known(Nba97GameClockReadWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameClockReadMachine* machine) {
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

static int validate(Nba97GameClockReadContext* context,
    Nba97GameClockReadProgress* out, Nba97GameClockReadRun* run) {
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

static int spend(Nba97GameClockReadRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate_clock(Nba97GameClockReadRun* run,
    uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, CLOCK_LW_PC, CLOCK_ADDRESS);
    TRY(spend(run));
    ++run->out->accesses;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        uint64_t offset = (uint64_t)CLOCK_ADDRESS - region->base;
        if (CLOCK_ADDRESS < region->base || offset > region->size ||
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

static int read_clock(Nba97GameClockReadRun* run) {
    Nba97GameClockReadWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate_clock(run, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (8u * i);
        if (!known || known[i])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (1u << i));
    }
    R(NBA97_MATCH_INITIALIZE_V0) = loaded;
    ++run->out->reads;
    {
        size_t index = run->out->access_events++;
        if (index < run->context->access_journal_capacity) {
            Nba97GameClockReadAccess* event =
                &run->context->access_journal[index];
            event->pc = CLOCK_LW_PC;
            event->address = CLOCK_ADDRESS;
            event->value = loaded.word;
            event->operation = run->out->operations;
            event->width = 4;
            event->known_mask = loaded.known_mask;
            event->kind = NBA97_GAME_CLOCK_READ_READ;
        }
    }
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_clock_read(Nba97GameClockReadContext* context,
    Nba97GameClockReadProgress* out) {
    Nba97GameClockReadRun storage;
    Nba97GameClockReadRun* run = &storage;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800A5810: LUI exposes this exact prefix even when the
     * following retained-memory read cannot complete. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800d0000));

    /* 0x800A5814: the sole operation reads the raw counter and preserves
     * byte knownness without interpreting or advancing time. */
    TRY(read_clock(run));

    /* 0x800A5818..0x800A581C: JR consumes live ra independently of v0;
     * the NOP delay slot changes no machine state. */
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, CLOCK_JR_PC, R(NBA97_MATCH_INITIALIZE_RA).word);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
