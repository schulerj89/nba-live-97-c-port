#include "game_frame_interrupt_disable.h"

#include <string.h>

typedef struct Nba97GameFrameInterruptDisableRun {
    Nba97GameFrameInterruptDisableContext* context;
    Nba97GameFrameInterruptDisableProgress* out;
    Nba97GameFrameInterruptDisableMachine machine;
} Nba97GameFrameInterruptDisableRun;

#define REG(run, index) ((run)->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameFrameInterruptDisableRun* run) {
    run->out->machine = run->machine;
}

static void stop(Nba97GameFrameInterruptDisableRun* run, uint32_t pc) {
    run->out->stopped_pc = pc;
    publish(run);
}

static int machine_valid(
    const Nba97GameFrameInterruptDisableMachine* machine) {
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

static int validate(Nba97GameFrameInterruptDisableContext* context,
    Nba97GameFrameInterruptDisableProgress* out,
    Nba97GameFrameInterruptDisableRun* run) {
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

static int spend(Nba97GameFrameInterruptDisableRun* run, uint32_t pc) {
    stop(run, pc);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameFrameInterruptDisableRun* run, uint8_t kind,
    uint32_t pc, const Nba97GameFrameInterruptDisableWord* value) {
    size_t index = run->out->journal_events++;
    if (index < run->context->journal_capacity) {
        Nba97GameFrameInterruptDisableJournal* event =
            &run->context->journal[index];
        event->pc = pc;
        event->value = value->word;
        event->operation = run->out->operations;
        event->known_mask = value->known_mask;
        event->kind = kind;
    }
}

static int read_status(Nba97GameFrameInterruptDisableRun* run,
    uint32_t pc, Nba97GameFrameInterruptDisableWord* value) {
    TRY(spend(run, pc));
    *value = run->machine.cp0_status;
    ++run->out->cp0_reads;
    journal(run, NBA97_GAME_FRAME_INTERRUPT_DISABLE_CP0_READ, pc, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_status(Nba97GameFrameInterruptDisableRun* run,
    uint32_t pc, const Nba97GameFrameInterruptDisableWord* value) {
    TRY(spend(run, pc));
    run->machine.cp0_status = *value;
    ++run->out->cp0_writes;
    journal(run, NBA97_GAME_FRAME_INTERRUPT_DISABLE_CP0_WRITE, pc, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_frame_interrupt_disable(
    Nba97GameFrameInterruptDisableContext* context,
    Nba97GameFrameInterruptDisableProgress* out) {
    Nba97GameFrameInterruptDisableRun storage;
    Nba97GameFrameInterruptDisableRun* run = &storage;
    Nba97GameFrameInterruptDisableWord status;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80048FF4 MFC0 copies the explicit live CP0 Status word and
     * its per-byte knownness into v0. This is the first budgeted operation. */
    TRY(read_status(run, UINT32_C(0x80048ff4), &status));
    REG(run, NBA97_MATCH_INITIALIZE_V0) = status;
    out->old_status = status;

    /* 0x80048FF8..0x80048FFC: LI makes v1 fully known -2, then AND clears
     * only bit 0. A source Status byte that was unknown remains unknown even
     * though its representative bit 0 is cleared. */
    REG(run, NBA97_MATCH_INITIALIZE_V1).word = UINT32_C(0xfffffffe);
    REG(run, NBA97_MATCH_INITIALIZE_V1).known_mask = 0x0fu;
    REG(run, NBA97_MATCH_INITIALIZE_V1).word &=
        REG(run, NBA97_MATCH_INITIALIZE_V0).word;
    REG(run, NBA97_MATCH_INITIALIZE_V1).known_mask =
        REG(run, NBA97_MATCH_INITIALIZE_V0).known_mask;
    out->new_status = REG(run, NBA97_MATCH_INITIALIZE_V1);
    publish(run);

    /* 0x80049000 MTC0 is the second budgeted operation. It publishes the
     * partially known result before return-address validity is consumed. */
    TRY(write_status(run, UINT32_C(0x80049000),
        &REG(run, NBA97_MATCH_INITIALIZE_V1)));

    /* 0x80049004 JR consumes live ra only after the CP0 write. An unknown ra
     * refuses here after the source NOP delay at 0x80049008. */
    if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80049004));
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0);
    return NBA97_TEXT_COMPLETE;
}
