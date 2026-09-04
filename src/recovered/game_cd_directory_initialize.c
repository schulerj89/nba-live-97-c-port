#include "game_cd_directory_initialize.h"

#include <string.h>

#define BUFFER UINT32_C(0x80103550)
#define CACHE_FLAG UINT32_C(0x800c4abc)
#define DISC_BASE UINT32_C(0x800ebc3c)
#define VOLUME_SECTOR UINT32_C(0x800fb150)
#define ROOT_LBA UINT32_C(0x800d7d3c)
#define ROOT_SIZE UINT32_C(0x800d7d40)

typedef struct Nba97GameCdDirectoryInitializeRun {
    Nba97GameCdDirectoryInitializeContext* context;
    Nba97GameCdDirectoryInitializeProgress* out;
    uint32_t sp;
} Nba97GameCdDirectoryInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameCdDirectoryInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameCdDirectoryInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameCdDirectoryInitializeRun* run, uint32_t address,
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

static int write_value(Nba97GameCdDirectoryInitializeRun* run,
    uint32_t address, size_t width, size_t alignment, uint32_t pc,
    uint32_t value, uint8_t value_known) {
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

static int write_word(Nba97GameCdDirectoryInitializeRun* run,
    uint32_t address, uint32_t pc, uint32_t value, uint8_t known) {
    return write_value(run, address, 4, 4, pc, value, known);
}

static int write_byte(Nba97GameCdDirectoryInitializeRun* run,
    uint32_t address, uint32_t pc, uint8_t value) {
    return write_value(run, address, 1, 1, pc, value, 1);
}

static int read_value(Nba97GameCdDirectoryInitializeRun* run,
    uint32_t address, size_t width, size_t alignment, uint32_t pc,
    uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    size_t i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    if (known)
        for (i = 0; i < width; ++i)
            if (!known[i])
                return NBA97_TEXT_UNKNOWN;
    for (i = 0; i < width; ++i)
        result |= (uint32_t)data[i] << (i * 8u);
    *value = result;
    ++run->out->reads;
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameCdDirectoryInitializeRun* run,
    uint32_t address, uint32_t pc, uint32_t* value) {
    return read_value(run, address, 4, 4, pc, value);
}

static int read_byte(Nba97GameCdDirectoryInitializeRun* run,
    uint32_t address, uint32_t pc, uint32_t* value) {
    return read_value(run, address, 1, 1, pc, value);
}

static int callback(Nba97GameCdDirectoryInitializeRun* run, uint8_t kind,
    uint32_t pc, uint32_t entry, uint32_t address, uint8_t argument_count,
    uint32_t a0, uint32_t a1,
    Nba97GameCdDirectoryInitializeValue* value) {
    Nba97GameCdDirectoryInitializeEvent event;
    int result;
    stop(run, pc, address, entry);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.address = address;
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
    if (kind == NBA97_CD_DIRECTORY_INITIALIZE_CALL)
        ++run->out->calls_completed;
    else
        ++run->out->poll_callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int call(Nba97GameCdDirectoryInitializeRun* run, uint32_t pc,
    uint32_t entry, uint8_t argument_count, uint32_t a0, uint32_t a1,
    Nba97GameCdDirectoryInitializeValue* value) {
    return callback(run, NBA97_CD_DIRECTORY_INITIALIZE_CALL, pc, entry, 0,
        argument_count, a0, a1, value);
}

static int poll(Nba97GameCdDirectoryInitializeRun* run, uint32_t address) {
    Nba97GameCdDirectoryInitializeValue ignored;
    return callback(run, NBA97_CD_DIRECTORY_INITIALIZE_POLL,
        0x80091c90u, 0, address, 0, 0, 0, &ignored);
}

static int validate(Nba97GameCdDirectoryInitializeContext* context,
    Nba97GameCdDirectoryInitializeProgress* out,
    Nba97GameCdDirectoryInitializeRun* run) {
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
    run->sp = context->stack_pointer - 0x30u;
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    return NBA97_TEXT_COMPLETE;
}

static int finish(Nba97GameCdDirectoryInitializeRun* run, uint32_t source_v0,
    uint8_t cached) {
    uint32_t value;
    TRY(read_word(run, run->sp + 0x2cu, 0x80091dccu, &value));
    run->out->restored_return_address = value;
    TRY(read_word(run, run->sp + 0x28u, 0x80091dd0u, &value));
    run->out->restored_frame_pointer = value;
    run->out->stack_pointer = run->sp + 0x30u;
    run->out->return_v0 = source_v0;
    run->out->cached = cached;
    run->out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_cd_directory_initialize(
    Nba97GameCdDirectoryInitializeContext* context,
    Nba97GameCdDirectoryInitializeProgress* out) {
    Nba97GameCdDirectoryInitializeRun storage;
    Nba97GameCdDirectoryInitializeRun* run = &storage;
    Nba97GameCdDirectoryInitializeValue value;
    uint32_t pointer;
    uint32_t status;
    uint32_t first;
    uint32_t second;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80091C08 prologue and the cached-entry test. */
    TRY(write_word(run, run->sp + 0x2cu, 0x80091c0cu,
        context->return_address, 1));
    TRY(write_word(run, run->sp + 0x28u, 0x80091c10u,
        context->frame_pointer, 1));
    TRY(read_word(run, CACHE_FLAG, 0x80091c1cu, &status));
    if (status)
        return finish(run, 1, 1);

    TRY(call(run, 0x80091c2cu, 0x800a4830u, 0, 0, 0, &value));
    TRY(call(run, 0x80091c34u, 0x800985a4u, 0, 0, 0, &value));
    TRY(call(run, 0x80091c3cu, 0x8009d94cu, 0, 0, 0, &value));
    TRY(write_word(run, run->sp + 0x10u, 0x80091c4cu, BUFFER, 1));
    TRY(read_word(run, run->sp + 0x10u, 0x80091c50u, &pointer));
    TRY(write_byte(run, pointer + 1u, 0x80091c58u, 0xffu));
    TRY(read_word(run, run->sp + 0x10u, 0x80091c5cu, &pointer));
    TRY(call(run, 0x80091c60u, 0x8009fa6cu, 1, pointer, 0, &value));
    TRY(write_word(run, run->sp + 0x20u, 0x80091c68u,
        value.word, value.known));
    TRY(read_word(run, run->sp + 0x20u, 0x80091c6cu, &status));
    if (!status)
        return finish(run, 0, 0);

    for (;;) {
        TRY(read_word(run, run->sp + 0x10u, 0x80091c88u, &pointer));
        TRY(read_byte(run, pointer + 1u, 0x80091c90u, &status));
        if (status != 0xffu)
            break;
        ++out->polls;
        if (out->polls > context->poll_budget) {
            stop(run, 0x80091c90u, pointer + 1u, 0);
            return NBA97_TEXT_LIMIT;
        }
        TRY(poll(run, pointer + 1u));
    }

    /* Preserve the four-byte stack tuple passed to the second 0x80091870:
     * descriptor minute/second followed by sector 0x16 and a zero terminator. */
    TRY(read_word(run, run->sp + 0x10u, 0x80091cb0u, &pointer));
    TRY(read_byte(run, pointer + 4u, 0x80091cbcu, &first));
    TRY(write_byte(run, run->sp + 0x18u, 0x80091cc4u, (uint8_t)first));
    TRY(read_word(run, run->sp + 0x10u, 0x80091cc8u, &pointer));
    TRY(read_byte(run, pointer + 5u, 0x80091cd4u, &second));
    TRY(write_byte(run, run->sp + 0x19u, 0x80091cdcu, (uint8_t)second));
    TRY(write_byte(run, run->sp + 0x1au, 0x80091ce4u, 0x16u));
    TRY(write_byte(run, run->sp + 0x1bu, 0x80091ce8u, 0));

    TRY(read_word(run, run->sp + 0x10u, 0x80091cecu, &pointer));
    TRY(call(run, 0x80091cfcu, 0x80091870u, 1, pointer + 4u, 0, &value));
    TRY(write_word(run, DISC_BASE, 0x80091d08u, value.word, value.known));
    out->disc_base_sector = value.word;
    out->disc_base_sector_known = value.known;
    TRY(call(run, 0x80091d14u, 0x80091870u, 1,
        run->sp + 0x18u, 0, &value));
    TRY(write_word(run, VOLUME_SECTOR, 0x80091d20u,
        value.word, value.known));
    out->primary_volume_sector = value.word;
    out->primary_volume_sector_known = value.known;
    TRY(read_word(run, VOLUME_SECTOR, 0x80091d28u, &second));
    TRY(read_word(run, DISC_BASE, 0x80091d30u, &first));
    TRY(call(run, 0x80091d40u, 0x80091e1cu, 1,
        second - first, 0, &value));
    TRY(call(run, 0x80091d54u, 0x80091e80u, 2,
        BUFFER, 1, &value));

    TRY(write_word(run, run->sp + 0x24u, 0x80091d64u, BUFFER, 1));
    TRY(read_word(run, run->sp + 0x24u, 0x80091d68u, &pointer));
    TRY(call(run, 0x80091d7cu, 0x800aa04cu, 2,
        pointer + 0x9eu, 4, &value));
    TRY(write_word(run, ROOT_LBA, 0x80091d88u, value.word, value.known));
    out->root_directory_lba = value.word;
    out->root_directory_lba_known = value.known;
    TRY(read_word(run, run->sp + 0x24u, 0x80091d8cu, &pointer));
    TRY(call(run, 0x80091da0u, 0x800aa04cu, 2,
        pointer + 0xa6u, 4, &value));
    TRY(write_word(run, ROOT_SIZE, 0x80091dacu,
        value.word, value.known));
    out->root_directory_size = value.word;
    out->root_directory_size_known = value.known;
    TRY(write_word(run, CACHE_FLAG, 0x80091db8u, 1, 1));
    return finish(run, 1, 0);
}
