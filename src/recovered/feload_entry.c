#include "feload_entry.h"

#include <string.h>

static const uint32_t BSS_BEGIN = UINT32_C(0x801e903c);
static const uint32_t BSS_END = UINT32_C(0x801eb088);
static const uint32_t MEMORY_TOP = UINT32_C(0x801e8b70);
static const uint32_t STACK_RESERVE = UINT32_C(0x801e8b6c);
static const uint32_t HEAP_SIZE_GLOBAL = UINT32_C(0x801e8b50);
static const uint32_t HEAP_BASE_GLOBAL = UINT32_C(0x801e8b4c);

typedef struct Nba97FeloadEntryRun {
    Nba97FeloadEntryContext* context;
    Nba97FeloadEntryProgress* out;
    Nba97FeloadEntryRegisters registers;
} Nba97FeloadEntryRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static Nba97FeloadEntryRegister known_register(uint32_t word) {
    Nba97FeloadEntryRegister value;
    value.word = word;
    value.known = 1;
    return value;
}

static void sync(Nba97FeloadEntryRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97FeloadEntryRun* run, uint32_t pc, uint32_t address,
    uint32_t entry) {
    sync(run);
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int valid_registers(const Nba97FeloadEntryRegisters* registers) {
    unsigned i;
    for (i = 1; i < NBA97_FELOAD_REGISTER_COUNT; ++i) {
        const Nba97FeloadEntryRegister* value = &registers->gpr[i];
        if (value->known > 1 || (!value->known && value->word))
            return 0;
    }
    return 1;
}

static void normalize_zero(Nba97FeloadEntryRegisters* registers) {
    registers->gpr[NBA97_FELOAD_R_ZERO] = known_register(0);
}

static int spend(Nba97FeloadEntryRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97FeloadEntryRun* run, uint32_t address, uint32_t pc,
    uint8_t** data, uint8_t** known) {
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

static void observe(Nba97FeloadEntryRun* run, uint32_t pc,
    uint32_t address, uint32_t value, uint8_t known_mask, uint8_t kind) {
    Nba97FeloadEntryAccess access;
    if (!run->context->observe_access)
        return;
    access.pc = pc;
    access.address = address;
    access.value = value;
    access.width = 4;
    access.known_mask = known_mask;
    access.kind = kind;
    run->context->observe_access(run->context->user, &access);
}

static int read_word(Nba97FeloadEntryRun* run, uint32_t address,
    uint32_t pc, Nba97FeloadEntryRegister* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t word = 0;
    uint8_t known_mask = 0;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            known_mask = (uint8_t)(known_mask | (uint8_t)(1u << i));
    }
    ++run->out->reads;
    observe(run, pc, address, word, known_mask, NBA97_FELOAD_ENTRY_READ);
    if (known_mask == 0x0fu) {
        *value = known_register(word);
    } else {
        value->word = 0;
        value->known = 0;
    }
    return NBA97_TEXT_COMPLETE;
}

static int read_known_word(Nba97FeloadEntryRun* run, uint32_t address,
    uint32_t pc, Nba97FeloadEntryRegister* value) {
    TRY(read_word(run, address, pc, value));
    if (!value->known)
        return NBA97_TEXT_UNKNOWN;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97FeloadEntryRun* run, uint32_t address,
    uint32_t pc, Nba97FeloadEntryRegister value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && !value.known)
        return NBA97_TEXT_UNKNOWN;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value.word >> (i * 8u));
        if (known)
            known[i] = value.known;
    }
    ++run->out->stores;
    observe(run, pc, address, value.word,
        value.known ? 0x0fu : 0, NBA97_FELOAD_ENTRY_WRITE);
    return NBA97_TEXT_COMPLETE;
}

static int signed_add_immediate(uint32_t value, int32_t immediate,
    uint32_t* result) {
    uint32_t addend = (uint32_t)immediate;
    uint32_t sum = value + addend;
    *result = sum;
    return ((value ^ sum) & (addend ^ sum) & UINT32_C(0x80000000)) == 0;
}

static int invoke(Nba97FeloadEntryRun* run, uint8_t kind, uint32_t pc,
    uint32_t entry, uint8_t argument_count,
    enum Nba97FeloadEntryCalleeOutcome* outcome) {
    Nba97FeloadEntryEvent event;
    Nba97FeloadEntryRegisters returned;
    int result;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.registers = run->registers;
    event.kind = kind;
    event.argument_count = argument_count;
    returned = event.registers;
    *outcome = NBA97_FELOAD_ENTRY_CALLEE_UNSET;
    result = run->context->io(run->context->user, &run->context->memory,
        &event, &returned, outcome);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (*outcome != NBA97_FELOAD_ENTRY_CALLEE_RETURNED &&
        *outcome != NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED)
        return NBA97_TEXT_ARGUMENT;
    if (!valid_registers(&returned))
        return NBA97_TEXT_ARGUMENT;
    normalize_zero(&returned);
    run->registers = returned;
    ++run->out->callbacks_completed;
    sync(run);
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97FeloadEntryContext* context,
    Nba97FeloadEntryProgress* out, Nba97FeloadEntryRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count) ||
        !valid_registers(&context->registers))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < context->memory.count; ++i) {
        const Nba97GameTextRegion* a = &context->memory.region[i];
        if (!a->data || !a->size ||
            a->size > UINT64_C(0x100000000) ||
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
    normalize_zero(&run->registers);
    sync(run);
    return NBA97_TEXT_COMPLETE;
}

static int finish_transfer(Nba97FeloadEntryRun* run) {
    run->out->transferred = 1;
    run->out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}

int nba97_feload_entry(Nba97FeloadEntryContext* context,
    Nba97FeloadEntryProgress* out) {
    Nba97FeloadEntryRun storage;
    Nba97FeloadEntryRun* run = &storage;
    Nba97FeloadEntryRegister value;
    enum Nba97FeloadEntryCalleeOutcome outcome;
    uint32_t cursor;
    uint32_t sum;
    TRY(validate(context, out, run));

    /* FELOAD 0x801E1410..0x801E1430 clears one word per iteration. The
     * unsigned SLTU/BNE condition makes 0x801EB088 the exclusive endpoint. */
    run->registers.gpr[NBA97_FELOAD_R_V0] = known_register(BSS_BEGIN);
    run->registers.gpr[NBA97_FELOAD_R_V1] = known_register(BSS_END);
    for (cursor = BSS_BEGIN; cursor < BSS_END; cursor += 4u) {
        run->registers.gpr[NBA97_FELOAD_R_V0] = known_register(cursor);
        TRY(write_word(run, cursor, UINT32_C(0x801e1420),
            known_register(0)));
        ++out->words_cleared;
        run->registers.gpr[NBA97_FELOAD_R_V0] = known_register(cursor + 4u);
        run->registers.gpr[NBA97_FELOAD_R_AT] = known_register(
            cursor + 4u < BSS_END ? 1u : 0u);
    }

    /* FELOAD 0x801E1434..0x801E1448 loads the configured top, applies the
     * trapping signed ADDI -8, and maps only the new sp into KSEG0. */
    run->registers.gpr[NBA97_FELOAD_R_V0] =
        known_register(UINT32_C(0x801f0000));
    TRY(read_known_word(run, MEMORY_TOP, UINT32_C(0x801e1438), &value));
    run->registers.gpr[NBA97_FELOAD_R_V0] = value;
    if (!signed_add_immediate(value.word, -8, &sum)) {
        stop(run, UINT32_C(0x801e1440), 0, 0);
        out->trapped = 1;
        return NBA97_FELOAD_ENTRY_ARITHMETIC_TRAP;
    }
    run->registers.gpr[NBA97_FELOAD_R_V0] = known_register(sum);
    run->registers.gpr[NBA97_FELOAD_R_T0] =
        known_register(UINT32_C(0x80000000));
    run->registers.gpr[NBA97_FELOAD_R_SP] =
        known_register(sum | UINT32_C(0x80000000));

    /* FELOAD 0x801E144C..0x801E1480 strips the high three address bits from
     * the BSS endpoint, computes the two wrapping SUBU heap terms, then stores
     * heap size before remapping and storing the heap base. */
    run->registers.gpr[NBA97_FELOAD_R_A0] =
        known_register(BSS_END & UINT32_C(0x1fffffff));
    run->registers.gpr[NBA97_FELOAD_R_V1] =
        known_register(UINT32_C(0x801f0000));
    TRY(read_known_word(run, STACK_RESERVE, UINT32_C(0x801e1460), &value));
    run->registers.gpr[NBA97_FELOAD_R_V1] = value;
    run->registers.gpr[NBA97_FELOAD_R_A1] = known_register(
        run->registers.gpr[NBA97_FELOAD_R_V0].word - value.word -
        run->registers.gpr[NBA97_FELOAD_R_A0].word);
    out->heap_size = run->registers.gpr[NBA97_FELOAD_R_A1].word;
    run->registers.gpr[NBA97_FELOAD_R_AT] =
        known_register(UINT32_C(0x801f0000));
    TRY(write_word(run, HEAP_SIZE_GLOBAL, UINT32_C(0x801e1474),
        run->registers.gpr[NBA97_FELOAD_R_A1]));
    run->registers.gpr[NBA97_FELOAD_R_A0].word |= UINT32_C(0x80000000);
    out->heap_base = run->registers.gpr[NBA97_FELOAD_R_A0].word;
    run->registers.gpr[NBA97_FELOAD_R_AT] =
        known_register(UINT32_C(0x801f0000));
    TRY(write_word(run, HEAP_BASE_GLOBAL, UINT32_C(0x801e1480),
        run->registers.gpr[NBA97_FELOAD_R_A0]));

    /* FELOAD 0x801E1484..0x801E1494 overwrites the first cleared word with
     * the incoming live ra, then replaces gp and s8 before either child. */
    out->saved_return_address =
        run->registers.gpr[NBA97_FELOAD_R_RA];
    run->registers.gpr[NBA97_FELOAD_R_AT] =
        known_register(UINT32_C(0x801f0000));
    TRY(write_word(run, BSS_BEGIN, UINT32_C(0x801e1488),
        out->saved_return_address));
    run->registers.gpr[NBA97_FELOAD_R_GP] = known_register(BSS_BEGIN);
    run->registers.gpr[NBA97_FELOAD_R_S8] =
        run->registers.gpr[NBA97_FELOAD_R_SP];

    /* FELOAD 0x801E1498 call: JAL publishes ra first, and trapping ADDI a0+4
     * in its delay slot completes before child 0x801E1590 observes registers. */
    run->registers.gpr[NBA97_FELOAD_R_RA] =
        known_register(UINT32_C(0x801e14a0));
    if (!signed_add_immediate(run->registers.gpr[NBA97_FELOAD_R_A0].word,
            4, &sum)) {
        stop(run, UINT32_C(0x801e149c), 0, UINT32_C(0x801e1590));
        out->trapped = 1;
        return NBA97_FELOAD_ENTRY_ARITHMETIC_TRAP;
    }
    run->registers.gpr[NBA97_FELOAD_R_A0] = known_register(sum);
    out->delay_slot_completed = 1;
    TRY(invoke(run, NBA97_FELOAD_ENTRY_CHILD_801E1590,
        UINT32_C(0x801e1498), UINT32_C(0x801e1590), 2, &outcome));
    out->first_child_entered = 1;
    if (outcome == NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED)
        return finish_transfer(run);

    /* FELOAD 0x801E14A0..0x801E14A8 deliberately reloads the mutable saved-ra
     * word after the first child, even though the following JAL replaces ra. */
    run->registers.gpr[NBA97_FELOAD_R_RA] =
        known_register(UINT32_C(0x801f0000));
    TRY(read_word(run, BSS_BEGIN, UINT32_C(0x801e14a4), &value));
    run->registers.gpr[NBA97_FELOAD_R_RA] = value;
    out->restored_return_address = value;

    /* FELOAD 0x801E14AC uses the full live register state left by child one.
     * Its NOP delay slot changes nothing; JAL itself supplies ra=0x801E14B4. */
    run->registers.gpr[NBA97_FELOAD_R_RA] =
        known_register(UINT32_C(0x801e14b4));
    TRY(invoke(run, NBA97_FELOAD_ENTRY_CHILD_801E136C,
        UINT32_C(0x801e14ac), UINT32_C(0x801e136c), 0, &outcome));
    out->second_child_entered = 1;
    if (outcome == NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED)
        return finish_transfer(run);

    /* FELOAD 0x801E14B4 is the authoritative end of this owner. A returning
     * second child reaches BREAK 1; bytes at 0x801E14B8 belong to the next
     * routine despite the decompiler's incorrect fallthrough. */
    stop(run, UINT32_C(0x801e14b4), 0, 0);
    out->trapped = 1;
    return NBA97_FELOAD_ENTRY_BREAK_TRAP;
}
