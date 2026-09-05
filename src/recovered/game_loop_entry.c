#include "game_loop_entry.h"

#include <string.h>

typedef struct Nba97GameLoopEntryRun {
    Nba97GameLoopEntryContext* context;
    Nba97GameLoopEntryProgress* out;
    Nba97GameMatchInitializeRegisters registers;
} Nba97GameLoopEntryRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameLoopEntryRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameLoopEntryRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameLoopEntryRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameLoopEntryRun* run, uint8_t kind,
    uint32_t pc, uint32_t address,
    const Nba97GameMatchInitializeWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameLoopEntryAccess* event =
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

static int locate(Nba97GameLoopEntryRun* run, uint32_t address,
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

static int write_word(Nba97GameLoopEntryRun* run, uint32_t address,
    uint32_t pc, const Nba97GameMatchInitializeWord* value) {
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
    journal(run, NBA97_GAME_LOOP_ENTRY_STORE, pc, address, value);
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameLoopEntryRun* run, uint32_t address,
    uint32_t pc, Nba97GameMatchInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameMatchInitializeWord loaded;
    unsigned i;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_LOOP_ENTRY_READ, pc, address, value);
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

static int validate(Nba97GameLoopEntryContext* context,
    Nba97GameLoopEntryProgress* out, Nba97GameLoopEntryRun* run) {
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

static int require_known(Nba97GameLoopEntryRun* run, unsigned index,
    uint32_t pc) {
    if (run->registers.gpr[index].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int invoke_match_tick(Nba97GameLoopEntryRun* run) {
    Nba97GameLoopEntryEvent event;
    int accepted;
    /* JAL at 0x8002DC40 assigns ra before the 0x8002DC44 NOP. A budget
     * refusal cannot undo that architecturally completed register write. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word =
        UINT32_C(0x8002dc48);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    stop(run, UINT32_C(0x8002dc40), 0, UINT32_C(0x80068bf8));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = UINT32_C(0x8002dc40);
    event.delay_slot_pc = UINT32_C(0x8002dc44);
    event.entry = UINT32_C(0x80068bf8);
    event.operation = run->out->operations;
    event.kind = NBA97_GAME_LOOP_ENTRY_MATCH_TICK;
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

int nba97_game_loop_entry(Nba97GameLoopEntryContext* context,
    Nba97GameLoopEntryProgress* out) {
    Nba97GameLoopEntryRun storage;
    Nba97GameLoopEntryRun* run = &storage;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8002DC38..0x8002DC44: allocate the wrapping 0x18-byte
     * guest frame, save raw ra knownness, then enter the sole child. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, UINT32_C(0x8002dc38)));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word -= 0x18u;
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    TRY(write_word(run, out->frame_stack_pointer + 0x10u,
        UINT32_C(0x8002dc3c),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    TRY(invoke_match_tick(run));

    /* GAMEONLY 0x8002DC48..0x8002DC54: the child-mutable sp selects the live
     * ra slot. ADDIU and JR occur before their respective NOP delay slots. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, UINT32_C(0x8002dc48)));
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x10u,
        UINT32_C(0x8002dc48),
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 0x18u;
    if (out->restored_return_address.known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8002dc50), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
