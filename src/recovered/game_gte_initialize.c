#include "game_gte_initialize.h"

#include <string.h>

#define COP0_STATUS_CU2 UINT32_C(0x40000000)

typedef struct Nba97GameGteInitializeRun {
    Nba97GameGteInitializeContext* context;
    Nba97GameGteInitializeProgress* out;
} Nba97GameGteInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static int touch(Nba97GameGteInitializeRun* run, uint32_t pc,
    uint8_t target, uint8_t index) {
    run->out->stopped_pc = pc;
    run->out->stopped_target = target;
    run->out->stopped_register = index;
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int read_status(Nba97GameGteInitializeRun* run, uint32_t pc,
    uint32_t* value) {
    Nba97GameGteInitializeValue* status = &run->context->state->cop0_status;
    TRY(touch(run, pc, NBA97_GAME_GTE_TARGET_COP0_STATUS, 12));
    if (status->known > 1)
        return NBA97_TEXT_ARGUMENT;
    if (!status->known)
        return NBA97_TEXT_UNKNOWN;
    *value = status->word;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int write_status(Nba97GameGteInitializeRun* run, uint32_t pc,
    uint32_t value) {
    Nba97GameGteInitializeValue* status = &run->context->state->cop0_status;
    TRY(touch(run, pc, NBA97_GAME_GTE_TARGET_COP0_STATUS, 12));
    if (status->known > 1)
        return NBA97_TEXT_ARGUMENT;
    status->word = value;
    status->known = 1;
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int write_control(Nba97GameGteInitializeRun* run, uint32_t pc,
    uint8_t index, uint32_t value) {
    Nba97GameGteInitializeValue* target =
        &run->context->state->control[index];
    TRY(touch(run, pc, NBA97_GAME_GTE_TARGET_CONTROL, index));
    if (target->known > 1)
        return NBA97_TEXT_ARGUMENT;
    target->word = value;
    target->known = 1;
    ++run->out->stores;
    ++run->out->controls_written;
    run->out->control_written_mask |= UINT32_C(1) << index;
    return NBA97_TEXT_COMPLETE;
}

static int begin(Nba97GameGteInitializeContext* context,
    Nba97GameGteInitializeProgress* out,
    Nba97GameGteInitializeRun* run) {
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || !context->state)
        return NBA97_TEXT_ARGUMENT;
    run->context = context;
    run->out = out;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_gte_initialize(Nba97GameGteInitializeContext* context,
    Nba97GameGteInitializeProgress* out) {
    Nba97GameGteInitializeRun storage;
    Nba97GameGteInitializeRun* run = &storage;
    uint32_t status;
    TRY(begin(context, out, run));

    /* 0x8005667C..0x80056688 reads live CP0 Status, preserves its other
     * bits, enables GTE/CU2, and leaves the OR result live in v0. */
    TRY(read_status(run, 0x8005667cu, &status));
    out->status_before = status;
    status |= COP0_STATUS_CU2;
    out->return_v0 = status;
    out->return_v0_known = 1;
    TRY(write_status(run, 0x80056688u, status));
    out->status_after = status;

    /* These are the exact CTC2 writes at 0x80056694..0x800566D0. Keep ZSF3
     * and ZSF4 independent, and do not manufacture resets for other GTE
     * controls: the source deliberately leaves those registers live. */
    TRY(write_control(run, 0x80056694u, NBA97_GAME_GTE_ZSF3, 0x155u));
    TRY(write_control(run, 0x800566a0u, NBA97_GAME_GTE_ZSF4, 0x100u));
    TRY(write_control(run, 0x800566acu, NBA97_GAME_GTE_H, 1000u));
    TRY(write_control(run, 0x800566b8u, NBA97_GAME_GTE_DQA,
        UINT32_C(0xffffef9e)));
    TRY(write_control(run, 0x800566c4u, NBA97_GAME_GTE_DQB,
        UINT32_C(0x01400000)));
    TRY(write_control(run, 0x800566ccu, NBA97_GAME_GTE_OFX, 0));
    TRY(write_control(run, 0x800566d0u, NBA97_GAME_GTE_OFY, 0));

    out->completed = 1;
    out->stopped_pc = 0;
    out->stopped_target = NBA97_GAME_GTE_TARGET_NONE;
    out->stopped_register = 0;
    return NBA97_TEXT_COMPLETE;
}
