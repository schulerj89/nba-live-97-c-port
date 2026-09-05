#include "game_scene_load.h"

#include <string.h>

typedef struct Nba97GameSceneLoadRun {
    Nba97GameSceneLoadContext* context;
    Nba97GameSceneLoadProgress* out;
    Nba97GameSceneLoadRegisters registers;
} Nba97GameSceneLoadRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameSceneLoadRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameSceneLoadRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameSceneLoadRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameSceneLoadRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, const Nba97GameSceneLoadWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameSceneLoadAccess* event =
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

static int locate(Nba97GameSceneLoadRun* run, uint32_t address,
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

static int write_word(Nba97GameSceneLoadRun* run, uint32_t address,
    uint32_t pc, const Nba97GameSceneLoadWord* value) {
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
    journal(run, NBA97_GAME_SCENE_LOAD_STORE, pc, address, value);
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameSceneLoadRun* run, uint32_t address,
    uint32_t pc, Nba97GameSceneLoadWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameSceneLoadWord loaded;
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
    journal(run, NBA97_GAME_SCENE_LOAD_READ, pc, address, value);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(const Nba97GameSceneLoadRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameSceneLoadContext* context,
    Nba97GameSceneLoadProgress* out, Nba97GameSceneLoadRun* run) {
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

static int require_known(Nba97GameSceneLoadRun* run,
    unsigned index, uint32_t pc) {
    if (run->registers.gpr[index].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameSceneLoadRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind) {
    Nba97GameSceneLoadEvent event;
    int accepted;
    /* JAL assigns ra before the NOP delay slot and before native child entry.
     * A budget stop retains that source-visible register prefix. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = pc + 8u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.kind = kind;
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

int nba97_game_scene_load(Nba97GameSceneLoadContext* context,
    Nba97GameSceneLoadProgress* out) {
    Nba97GameSceneLoadRun storage;
    Nba97GameSceneLoadRun* run = &storage;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8002DB68..0x8002DB6C: ADDIU wraps at 32 bits, then SW
     * preserves entry ra byte knownness in the retained 0x18-byte frame. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, 0x8002db68u));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word -= 0x18u;
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    publish(run);
    TRY(write_word(run, out->frame_stack_pointer + 0x10u, 0x8002db6cu,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));

    /* GAMEONLY 0x8002DB70..0x8002DB7C: both JAL delay slots are NOPs.
     * The second child receives every mutation made by the first except ra,
     * which its JAL overwrites with 0x8002DB80. */
    TRY(invoke(run, 0x8002db70u, 0x800802acu,
        NBA97_GAME_SCENE_LOAD_CHILD_800802AC));
    TRY(invoke(run, 0x8002db78u, 0x80048d5cu,
        NBA97_GAME_SCENE_LOAD_CHILD_80048D5C));

    /* GAMEONLY 0x8002DB80..0x8002DB8C: resolve the saved word through the
     * final child's live sp, restore that sp, then require ra for JR. The JR
     * delay slot is a NOP and leaves the final child state unchanged. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, 0x8002db80u));
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x10u,
        0x8002db80u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 0x18u;
    publish(run);
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_RA, 0x8002db88u));
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
