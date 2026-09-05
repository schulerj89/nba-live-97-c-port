#include "game_match_initialize.h"

#include <string.h>

#define TEAM_0_ADDRESS UINT32_C(0x80021d74)
#define TEAM_1_ADDRESS UINT32_C(0x80021d78)
#define TEAM_0_SNAPSHOT_ADDRESS UINT32_C(0x80022084)
#define TEAM_1_SNAPSHOT_ADDRESS UINT32_C(0x80022adc)
#define MATCH_STATE_ADDRESS UINT32_C(0x800fdb4c)
#define MATCH_STATE_SIZE UINT32_C(0x00000e7c)
#define FINAL_FLAG_ADDRESS UINT32_C(0x80020c18)

typedef struct Nba97GameMatchInitializeRun {
    Nba97GameMatchInitializeContext* context;
    Nba97GameMatchInitializeProgress* out;
    Nba97GameMatchInitializeRegisters registers;
} Nba97GameMatchInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameMatchInitializeRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameMatchInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameMatchInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameMatchInitializeRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameMatchInitializeWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameMatchInitializeAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word;
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask = value->known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameMatchInitializeRun* run, uint32_t address,
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

static int read_word(Nba97GameMatchInitializeRun* run, uint32_t address,
    uint32_t pc, Nba97GameMatchInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameMatchInitializeWord loaded;
    unsigned i;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_MATCH_INITIALIZE_READ, pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameMatchInitializeRun* run, uint32_t address,
    uint32_t pc, const Nba97GameMatchInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_MATCH_INITIALIZE_STORE, pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(const Nba97GameMatchInitializeRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameMatchInitializeContext* context,
    Nba97GameMatchInitializeProgress* out,
    Nba97GameMatchInitializeRun* run) {
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

static int require_known(Nba97GameMatchInitializeRun* run,
    unsigned index, uint32_t pc) {
    if (run->registers.gpr[index].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameMatchInitializeRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    uint32_t delay_slot_pc, unsigned delay_register, uint32_t delay_value) {
    Nba97GameMatchInitializeEvent event;
    int accepted;
    /* JAL assigns ra before its delay slot. A native budget may stop entry to
     * the child, but it cannot undo either source instruction. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = pc + 8u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    if (delay_register < NBA97_MATCH_INITIALIZE_REGISTER_COUNT) {
        run->registers.gpr[delay_register].word = delay_value;
        run->registers.gpr[delay_register].known_mask = 0x0fu;
    }
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

int nba97_game_match_initialize(Nba97GameMatchInitializeContext* context,
    Nba97GameMatchInitializeProgress* out) {
    Nba97GameMatchInitializeRun storage;
    Nba97GameMatchInitializeRun* run = &storage;
    Nba97GameMatchInitializeWord value;
    static const uint32_t call_pc[11] = {
        UINT32_C(0x8002dbc8), UINT32_C(0x8002dbd0),
        UINT32_C(0x8002dbd8), UINT32_C(0x8002dbe0),
        UINT32_C(0x8002dbe8), UINT32_C(0x8002dbf0),
        UINT32_C(0x8002dbf8), UINT32_C(0x8002dc00),
        UINT32_C(0x8002dc08), UINT32_C(0x8002dc10),
        UINT32_C(0x8002dc20)
    };
    static const uint32_t call_entry[11] = {
        UINT32_C(0x80063d58), UINT32_C(0x80029114),
        UINT32_C(0x8007fd40), UINT32_C(0x800294f8),
        UINT32_C(0x8002ab30), UINT32_C(0x800640d8),
        UINT32_C(0x800659f0), UINT32_C(0x80065db0),
        UINT32_C(0x80031e00), UINT32_C(0x80038a18),
        UINT32_C(0x800763f4)
    };
    unsigned i;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8002DB90..0x8002DB9C: both team loads occur before the
     * stack pointer changes, and their byte knownness remains independent. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word =
        UINT32_C(0x80020000); /* LUI at 0x8002DB90. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 0x0fu;
    TRY(read_word(run, TEAM_0_ADDRESS, 0x8002db94u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    out->team_snapshot[0] =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1].word =
        UINT32_C(0x80020000); /* LUI at 0x8002DB98. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask = 0x0fu;
    TRY(read_word(run, TEAM_1_ADDRESS, 0x8002db9cu,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]));
    out->team_snapshot[1] =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V1];

    /* GAMEONLY 0x8002DBA0..0x8002DBBC: form the live 0x18-byte frame and
     * publish ra followed by the two raw snapshots without requiring them. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, 0x8002dba0u));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word -= 0x18u;
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word = MATCH_STATE_ADDRESS;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 0x0fu;
    TRY(write_word(run, out->frame_stack_pointer + 0x10u, 0x8002dbacu,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].word =
        UINT32_C(0x80020000); /* LUI at 0x8002DBB0. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].known_mask = 0x0fu;
    TRY(write_word(run, TEAM_0_SNAPSHOT_ADDRESS, 0x8002dbb4u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].word =
        UINT32_C(0x80020000); /* LUI at 0x8002DBB8. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].known_mask = 0x0fu;
    TRY(write_word(run, TEAM_1_SNAPSHOT_ADDRESS, 0x8002dbbcu,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]));

    /* The 0x8002DBC4 delay slot supplies the exact zero-owner byte count. */
    TRY(invoke(run, 0x8002dbc0u, 0x800a3a74u,
        NBA97_MATCH_INITIALIZE_MEMORY_ZERO, 2, 0x8002dbc4u,
        NBA97_MATCH_INITIALIZE_A1, MATCH_STATE_SIZE));

    /* GAMEONLY 0x8002DBC8..0x8002DC14: ten ordered calls. NOP delay slots
     * retain every live register; only the last delay slot assigns a0=-1. */
    for (i = 0; i < 10; ++i) {
        TRY(invoke(run, call_pc[i], call_entry[i],
            (uint8_t)(NBA97_MATCH_INITIALIZE_CHILD_80063D58 + i),
            call_pc[i] == 0x8002dc10u ? 1 : 0, call_pc[i] + 4u,
            call_pc[i] == 0x8002dc10u ? NBA97_MATCH_INITIALIZE_A0 :
                NBA97_MATCH_INITIALIZE_REGISTER_COUNT,
            UINT32_MAX));
    }

    /* GAMEONLY 0x8002DC18..0x8002DC24: AT forms the fixed flag address,
     * its zero store completes before the final subsystem child is entered. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].word = UINT32_C(0x80020000);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].known_mask = 0x0fu;
    value.word = 0;
    value.known_mask = 0x0fu;
    TRY(write_word(run, FINAL_FLAG_ADDRESS, 0x8002dc1cu, &value));
    TRY(invoke(run, call_pc[10], call_entry[10],
        NBA97_MATCH_INITIALIZE_CHILD_800763F4, 0, 0x8002dc24u,
        NBA97_MATCH_INITIALIZE_REGISTER_COUNT, 0));

    /* GAMEONLY 0x8002DC28..0x8002DC34: read ra through the live sp, then
     * restore that live sp. The final child's v0 and other GPRs pass through. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, 0x8002dc28u));
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x10u,
        0x8002dc28u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 0x18u;
    if (out->restored_return_address.known_mask != 0x0fu) {
        stop(run, 0x8002dc30u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
