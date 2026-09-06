#include "game_frame_interrupt_restore.h"

#include <string.h>

typedef struct Nba97GameFrameInterruptRestoreRun {
    Nba97GameFrameInterruptRestoreContext* context;
    Nba97GameFrameInterruptRestoreProgress* out;
    Nba97GameFrameInterruptRestoreMachine machine;
} Nba97GameFrameInterruptRestoreRun;

#define REG(run, index) ((run)->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameFrameInterruptRestoreRun* run) {
    run->out->machine = run->machine;
}

static void stop(Nba97GameFrameInterruptRestoreRun* run, uint32_t pc) {
    run->out->stopped_pc = pc;
    publish(run);
}

static int machine_valid(
    const Nba97GameFrameInterruptRestoreMachine* machine) {
    unsigned i;
    if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask !=
            0x0fu ||
        machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu ||
        machine->cp0_status.known_mask > 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine->registers.gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameFrameInterruptRestoreContext* context,
    Nba97GameFrameInterruptRestoreProgress* out,
    Nba97GameFrameInterruptRestoreRun* run) {
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->journal && context->journal_capacity) ||
        !machine_valid(&context->machine))
        return NBA97_TEXT_ARGUMENT;
    run->context = context;
    run->out = out;
    run->machine = context->machine;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int spend(Nba97GameFrameInterruptRestoreRun* run, uint32_t pc) {
    stop(run, pc);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameFrameInterruptRestoreRun* run, uint32_t pc,
    const Nba97GameFrameInterruptRestoreWord* value) {
    size_t index = run->out->journal_events++;
    if (index < run->context->journal_capacity) {
        Nba97GameFrameInterruptRestoreJournal* event =
            &run->context->journal[index];
        event->pc = pc;
        event->value = value->word;
        event->operation = run->out->operations;
        event->known_mask = value->known_mask;
        event->kind = NBA97_GAME_FRAME_INTERRUPT_RESTORE_CP0_WRITE;
    }
}

int nba97_game_frame_interrupt_restore(
    Nba97GameFrameInterruptRestoreContext* context,
    Nba97GameFrameInterruptRestoreProgress* out) {
    Nba97GameFrameInterruptRestoreRun storage;
    Nba97GameFrameInterruptRestoreRun* run = &storage;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8004900C MTC0 copies the complete a0 bit pattern and its
     * per-byte knownness into explicit CP0 Status. The previous Status value
     * is not read, merged, masked, or returned. */
    TRY(spend(run, UINT32_C(0x8004900c)));
    run->machine.cp0_status = REG(run, NBA97_MATCH_INITIALIZE_A0);
    out->published_status = run->machine.cp0_status;
    ++out->cp0_writes;
    journal(run, UINT32_C(0x8004900c), &run->machine.cp0_status);
    publish(run);

    /* 0x80049010 JR consumes live ra after the Status publication. Unknown
     * ra refuses here after the source NOP delay at 0x80049014. */
    if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80049010));
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0);
    return NBA97_TEXT_COMPLETE;
}
