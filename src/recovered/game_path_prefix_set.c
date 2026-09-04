#include "game_path_prefix_set.h"

#include <string.h>

#define DESTINATION UINT32_C(0x800d6dac)

typedef struct Nba97GamePathPrefixSetRun {
    Nba97GamePathPrefixSetContext* context;
    Nba97GamePathPrefixSetProgress* out;
    uint32_t sp;
} Nba97GamePathPrefixSetRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GamePathPrefixSetRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GamePathPrefixSetRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GamePathPrefixSetRun* run, uint32_t address,
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

static int write_value(Nba97GamePathPrefixSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t value,
    uint8_t value_known) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    if (value_known > 1)
        return NBA97_TEXT_ARGUMENT;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = value_known;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GamePathPrefixSetRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    return write_value(run, address, 4, 4, pc, value, 1);
}

static int write_byte(Nba97GamePathPrefixSetRun* run, uint32_t address,
    uint32_t pc, uint8_t value, uint8_t known) {
    return write_value(run, address, 1, 1, pc, value, known);
}

static int read_value(Nba97GamePathPrefixSetRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t* value,
    uint8_t* value_known) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    size_t i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    *value_known = 1;
    for (i = 0; i < width; ++i) {
        result |= (uint32_t)data[i] << (i * 8u);
        if (known && !known[i])
            *value_known = 0;
    }
    *value = result;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GamePathPrefixSetRun* run, uint32_t address,
    uint32_t pc, uint32_t* value) {
    uint8_t known;
    TRY(read_value(run, address, 4, 4, pc, value, &known));
    return known ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
}

static int read_byte(Nba97GamePathPrefixSetRun* run, uint32_t address,
    uint32_t pc, uint32_t* value, uint8_t* known) {
    return read_value(run, address, 1, 1, pc, value, known);
}

static int callback(Nba97GamePathPrefixSetRun* run, uint8_t kind,
    uint32_t pc, uint32_t entry, uint8_t argument_count,
    uint32_t a0, uint32_t a1, Nba97GamePathPrefixSetValue* value) {
    Nba97GamePathPrefixSetEvent event;
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
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = pc + 8u;
    event.kind = kind;
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

static int validate(Nba97GamePathPrefixSetContext* context,
    Nba97GamePathPrefixSetProgress* out,
    Nba97GamePathPrefixSetRun* run) {
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
    run->sp = context->stack_pointer - 0x18u;
    out->source_address = context->source_address;
    out->destination_address = DESTINATION;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

static int finish(Nba97GamePathPrefixSetRun* run, uint32_t return_v0,
    uint8_t return_known, uint8_t appended) {
    uint32_t value;
    TRY(read_word(run, run->sp + 0x14u, 0x800a3638u, &value));
    run->out->restored_return_address = value;
    TRY(read_word(run, run->sp + 0x10u, 0x800a363cu, &value));
    run->out->restored_register_s0 = value;
    run->out->stack_pointer = run->sp + 0x18u;
    run->out->return_v0 = return_v0;
    run->out->return_v0_known = return_known;
    run->out->separator_appended = appended;
    run->out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_path_prefix_set(Nba97GamePathPrefixSetContext* context,
    Nba97GamePathPrefixSetProgress* out) {
    Nba97GamePathPrefixSetRun storage;
    Nba97GamePathPrefixSetRun* run = &storage;
    Nba97GamePathPrefixSetValue value;
    uint32_t last_address;
    uint32_t last_byte;
    uint32_t separator;
    uint32_t terminator;
    uint8_t last_known;
    uint8_t separator_known;
    uint8_t terminator_known;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800A35D8 prologue and the two BIOS string boundaries. */
    TRY(write_word(run, run->sp + 0x10u, 0x800a35e0u,
        context->saved_register_s0));
    TRY(write_word(run, run->sp + 0x14u, 0x800a35ecu,
        context->return_address));
    TRY(callback(run, NBA97_GAME_PATH_PREFIX_COPY, 0x800a35f0u,
        0x8009cb6cu, 2, DESTINATION, context->source_address, &value));
    TRY(callback(run, NBA97_GAME_PATH_PREFIX_LENGTH, 0x800a35f8u,
        0x8009cb4cu, 1, DESTINATION, 0, &value));
    if (!value.known) {
        stop(run, 0x800a3604u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->copied_length = value.word;
    out->final_length = value.word;
    if (!value.word)
        return finish(run, DESTINATION - 1u, 1, 0);

    /* 0x800A3608 computes destination+length-1 with wrapping ADDU. */
    last_address = value.word + (DESTINATION - 1u);
    TRY(read_byte(run, last_address, 0x800a3610u,
        &last_byte, &last_known));
    if (!last_known)
        return NBA97_TEXT_UNKNOWN;
    if (last_byte == 0x5cu || last_byte == 0x3au)
        return finish(run, 0x3au, 1, 0);

    /* Source LB/SB pairs propagate unknown separator bytes without inventing
     * data. With the original gp these bytes are '\\' and NUL. */
    TRY(read_byte(run, context->global_pointer + 0x44u,
        0x800a3628u, &separator, &separator_known));
    TRY(read_byte(run, context->global_pointer + 0x45u,
        0x800a362cu, &terminator, &terminator_known));
    TRY(write_byte(run, last_address + 1u, 0x800a3630u,
        (uint8_t)separator, separator_known));
    TRY(write_byte(run, last_address + 2u, 0x800a3634u,
        (uint8_t)terminator, terminator_known));
    out->final_length = value.word + 1u;
    return finish(run, separator & 0x80u ? separator | UINT32_C(0xffffff00) : separator,
        separator_known, 1);
}
