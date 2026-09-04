#include "game_clock_initialize.h"

#include <limits.h>
#include <string.h>

#define INITIALIZATION_GUARD UINT32_C(0x800c4aa4)
#define CALLBACK_TABLE UINT32_C(0x800d6dec)
#define DIVIDER_COUNTER UINT32_C(0x800d7a78)
#define EFFECTIVE_RATE UINT32_C(0x800d7a94)
#define TIMER_TARGET UINT32_C(0x800d7a98)
#define ENTER_CRITICAL_SECTION_ENTRY UINT32_C(0x80098394)
#define INTERRUPT_CALLBACK_ENTRY UINT32_C(0x8009860c)
#define EXIT_HANDLER_REGISTER_ENTRY UINT32_C(0x800a575c)
#define SET_RCNT_ENTRY UINT32_C(0x800983b4)
#define START_RCNT_ENTRY UINT32_C(0x80098488)
#define EXIT_CRITICAL_SECTION_ENTRY UINT32_C(0x80098594)
#define CLOCK_COUNTER_RESET_ENTRY UINT32_C(0x800a5880)
#define CLOCK_INTERRUPT_HANDLER UINT32_C(0x800916b4)
#define CLOCK_SHUTDOWN_HANDLER UINT32_C(0x8009167c)
#define CLOCK_BASE UINT32_C(0x00409980)
#define CLOCK_COUNTER_SPEC UINT32_C(0xf2000002)
#define CLOCK_COUNTER_MODE UINT32_C(0x1000)

typedef struct Nba97GameClockInitializeRun {
    Nba97GameClockInitializeContext* context;
    Nba97GameClockInitializeProgress* out;
    uint32_t sp;
    uint32_t fp;
} Nba97GameClockInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameClockInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameClockInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameClockInitializeRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint8_t** data,
    uint8_t** known) {
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

static int read_word(Nba97GameClockInitializeRun* run, uint32_t address,
    uint32_t pc, uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (known)
        for (i = 0; i < 4; ++i)
            if (!known[i])
                return NBA97_TEXT_UNKNOWN;
    for (i = 0; i < 4; ++i)
        result |= (uint32_t)data[i] << (i * 8u);
    *value = result;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameClockInitializeRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int direct_call(Nba97GameClockInitializeRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count, uint32_t a0, uint32_t a1,
    uint32_t a2, Nba97GameClockInitializeValue* value) {
    Nba97GameClockInitializeEvent event;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.argument[0] = a0;
    event.argument[1] = a1;
    event.argument[2] = a2;
    event.stack_pointer = run->sp;
    event.frame_pointer = run->fp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = pc + 8u;
    event.argument_count = argument_count;
    value->word = 0;
    value->known = 0;
    result = run->context->io(run->context->user,
        &run->context->memory, &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int signed_divide(Nba97GameClockInitializeRun* run,
    uint32_t numerator, uint32_t denominator, uint32_t zero_pc,
    uint32_t overflow_pc, uint32_t* quotient) {
    int32_t signed_numerator = (int32_t)numerator;
    int32_t signed_denominator = (int32_t)denominator;
    if (!signed_denominator) {
        stop(run, zero_pc, 0, 0);
        run->out->trap_code = 7;
        return NBA97_GAME_CLOCK_DIVIDE_TRAP;
    }
    /* GAMEONLY retains the compiler's BREAK 6 check even though this fixed
     * positive numerator makes the INT_MIN/-1 case unreachable. */
    if (signed_numerator == INT32_MIN && signed_denominator == -1) {
        stop(run, overflow_pc, 0, 0);
        run->out->trap_code = 6;
        return NBA97_GAME_CLOCK_DIVIDE_TRAP;
    }
    *quotient = (uint32_t)(signed_numerator / signed_denominator);
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameClockInitializeContext* context,
    Nba97GameClockInitializeProgress* out,
    Nba97GameClockInitializeRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count))
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
    run->sp = context->stack_pointer - 0x20u;
    run->fp = context->frame_pointer;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->incoming_rate = context->requested_rate;
    out->clock_base = CLOCK_BASE;
    out->interrupt_handler = CLOCK_INTERRUPT_HANDLER;
    out->shutdown_handler = CLOCK_SHUTDOWN_HANDLER;
    out->root_counter_spec = CLOCK_COUNTER_SPEC;
    out->root_counter_mode = CLOCK_COUNTER_MODE;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_clock_initialize(Nba97GameClockInitializeContext* context,
    Nba97GameClockInitializeProgress* out) {
    Nba97GameClockInitializeRun storage;
    Nba97GameClockInitializeRun* run = &storage;
    Nba97GameClockInitializeValue value;
    uint32_t guard;
    uint32_t index;
    uint32_t divisor;
    uint32_t target;
    uint32_t effective;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800914D8 prologue. The a0 home slot is deliberately live:
     * cold-path callbacks can rewrite it before the reload at 0x800915A4. */
    TRY(write_word(run, run->sp + 0x1cu, 0x800914dcu,
        context->return_address));
    TRY(write_word(run, run->sp + 0x18u, 0x800914e0u, run->fp));
    run->fp = run->sp;
    TRY(write_word(run, run->fp + 0x20u, 0x800914e8u,
        context->requested_rate));
    TRY(direct_call(run, 0x800914ecu, ENTER_CRITICAL_SECTION_ENTRY, 0,
        0, 0, 0, &value));

    TRY(read_word(run, INITIALIZATION_GUARD, 0x800914f8u, &guard));
    out->initialization_guard_before = (uint8_t)(guard != 0);
    if (!guard) {
        TRY(write_word(run, DIVIDER_COUNTER, 0x8009150cu, 0));
        TRY(write_word(run, run->fp + 0x10u, 0x80091510u, 0));
        for (;;) {
            TRY(read_word(run, run->fp + 0x10u, 0x80091514u, &index));
            if ((int32_t)index >= 8)
                break;
            TRY(read_word(run, run->fp + 0x10u, 0x80091530u, &index));
            TRY(write_word(run, CALLBACK_TABLE + index * 4u,
                0x8009154cu, 0));
            ++out->callback_slots_cleared;
            TRY(read_word(run, run->fp + 0x10u, 0x80091550u, &index));
            TRY(write_word(run, run->fp + 0x10u, 0x80091560u,
                index + 1u));
        }
        TRY(direct_call(run, 0x80091578u, INTERRUPT_CALLBACK_ENTRY, 2,
            6, CLOCK_INTERRUPT_HANDLER, 0, &value));
        TRY(write_word(run, INITIALIZATION_GUARD, 0x80091588u, 1));
        TRY(direct_call(run, 0x80091594u, EXIT_HANDLER_REGISTER_ENTRY, 1,
            CLOCK_SHUTDOWN_HANDLER, 0, 0, &value));
        out->initialized_once = 1;
    }

    /* GAMEONLY 0x8009159C..0x80091624 uses signed DIV twice. Do not replace
     * this with a rounded or clamped rate: the published rate is quantized. */
    TRY(read_word(run, run->fp + 0x20u, 0x800915a4u, &divisor));
    out->live_rate_divisor = divisor;
    {
        int result = signed_divide(run, CLOCK_BASE, divisor,
            0x800915b8u, 0x800915d0u, &target);
        if (result != NBA97_TEXT_COMPLETE)
            return result;
    }
    out->timer_target = target;
    TRY(write_word(run, TIMER_TARGET, 0x800915dcu, target));
    TRY(read_word(run, TIMER_TARGET, 0x800915ecu, &target));
    {
        int result = signed_divide(run, CLOCK_BASE, target,
            0x80091600u, 0x80091618u, &effective);
        if (result != NBA97_TEXT_COMPLETE)
            return result;
    }
    out->effective_rate = effective;
    TRY(write_word(run, EFFECTIVE_RATE, 0x80091624u, effective));
    TRY(read_word(run, TIMER_TARGET, 0x80091634u, &target));
    out->timer_target = target;

    /* 0x80091628..0x80091660 starts Timer 2 and resets clock counters. Raw
     * SetRCnt/StartRCnt returns are observable here but ignored by the owner. */
    TRY(direct_call(run, 0x8009163cu, SET_RCNT_ENTRY, 3,
        CLOCK_COUNTER_SPEC, target, CLOCK_COUNTER_MODE, &value));
    out->set_rcnt_return = value.word;
    out->set_rcnt_return_known = value.known;
    TRY(direct_call(run, 0x8009164cu, START_RCNT_ENTRY, 1,
        CLOCK_COUNTER_SPEC, 0, 0, &value));
    out->start_rcnt_return = value.word;
    out->start_rcnt_return_known = value.known;
    TRY(direct_call(run, 0x80091654u, EXIT_CRITICAL_SECTION_ENTRY, 0,
        0, 0, 0, &value));
    TRY(direct_call(run, 0x8009165cu, CLOCK_COUNTER_RESET_ENTRY, 0,
        0, 0, 0, &value));
    out->return_v0 = value.word;
    out->return_v0_known = value.known;

    /* GAMEONLY 0x80091664..0x8009167B reloads the live saved words. */
    run->sp = run->fp;
    TRY(read_word(run, run->sp + 0x1cu, 0x80091668u,
        &out->restored_return_address));
    TRY(read_word(run, run->sp + 0x18u, 0x8009166cu,
        &out->restored_frame_pointer));
    run->sp += 0x20u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
