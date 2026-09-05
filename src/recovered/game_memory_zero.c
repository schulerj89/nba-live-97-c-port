#include "game_memory_zero.h"

#include <string.h>

typedef struct Nba97GameMemoryZeroRun {
    Nba97GameMemoryZeroContext* context;
    Nba97GameMemoryZeroProgress* out;
    uint32_t destination;
    uint32_t count;
} Nba97GameMemoryZeroRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static int negative(uint32_t value) {
    return (value & UINT32_C(0x80000000)) != 0;
}

static void stop(Nba97GameMemoryZeroRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->working_destination = run->destination;
    run->out->working_count = run->count;
}

static int locate(Nba97GameMemoryZeroRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint8_t** data,
    uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
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

static int write_zero(Nba97GameMemoryZeroRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        data[i] = 0;
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    run->out->bytes_stored += width;
    return NBA97_TEXT_COMPLETE;
}

static int validate(Nba97GameMemoryZeroContext* context,
    Nba97GameMemoryZeroProgress* out, Nba97GameMemoryZeroRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count) ||
        context->incoming_v0_known > 1)
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
    run->destination = context->destination;
    run->count = context->length;
    out->destination = context->destination;
    out->requested_length = context->length;
    out->working_destination = context->destination;
    out->working_count = context->length;
    out->return_v0 = context->incoming_v0;
    out->return_v0_known = context->incoming_v0_known;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_memory_zero(Nba97GameMemoryZeroContext* context,
    Nba97GameMemoryZeroProgress* out) {
    Nba97GameMemoryZeroRun storage;
    Nba97GameMemoryZeroRun* run = &storage;
    unsigned i;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800A3A74 sets a2=0 and falls through 0x800A3A78. The
     * source uses signed SLTI here, even though all address/count arithmetic
     * below wraps as 32-bit unsigned MIPS arithmetic. */
    if (negative(run->count) || run->count < 4u) {
        out->used_small_path = 1;
        run->count -= 1u; /* 0x800A3B98. */
        out->working_count = run->count;

        /* 0x800A3BA0 is the delay slot of BLTZ. It executes even for length
         * zero or a signed-negative count: this one-byte over-clear is an
         * original-game bug, not a native safety convenience. */
        TRY(write_zero(run, run->destination, 1, 1, 0x800a3ba0u));
        if (!negative(run->count)) {
            for (;;) {
                run->count -= 1u; /* 0x800A3BA4. */
                run->destination += 1u; /* 0x800A3BAC delay slot. */
                out->working_destination = run->destination;
                out->working_count = run->count;
                if (negative(run->count))
                    break;
                TRY(write_zero(run, run->destination, 1, 1,
                    0x800a3ba0u));
            }
        }
    } else {
        uint32_t partial;
        uint32_t effective;
        size_t width;

        /* Little-endian SWR clears destination through the end of its aligned
         * word. It is a partial unaligned store, not a read/modify/write. */
        partial = run->destination & 3u;
        width = 4u - partial;
        TRY(write_zero(run, run->destination, width, 1, 0x800a3a94u));
        partial = 4u - partial;
        run->destination += partial;
        run->count -= partial;

        run->count -= 0x80u;
        while (!negative(run->count)) {
            /* GAMEONLY 0x800A3AB8..0x800A3B34: exactly 32 explicit SWs. */
            for (i = 0; i < 32; ++i)
                TRY(write_zero(run, run->destination + i * 4u, 4, 4,
                    0x800a3ab8u + i * 4u));
            run->count -= 0x80u;
            run->destination += 0x80u; /* BGEZ delay slot at 0x800A3B40. */
            out->working_destination = run->destination;
            out->working_count = run->count;
        }

        run->count += 0x70u;
        while (!negative(run->count)) {
            /* GAMEONLY 0x800A3B50..5C: four separately observable SWs. */
            for (i = 0; i < 4; ++i)
                TRY(write_zero(run, run->destination + i * 4u, 4, 4,
                    0x800a3b50u + i * 4u));
            run->count -= 0x10u;
            run->destination += 0x10u; /* BGEZ delay slot at 0x800A3B68. */
            out->working_destination = run->destination;
            out->working_count = run->count;
        }

        run->count += 0x0cu;
        while (!negative(run->count)) {
            TRY(write_zero(run, run->destination, 4, 4, 0x800a3b78u));
            run->count -= 4u;
            run->destination += 4u; /* BGEZ delay slot at 0x800A3B84. */
            out->working_destination = run->destination;
            out->working_count = run->count;
        }

        run->destination += run->count; /* 0x800A3B88. */
        out->working_destination = run->destination;
        out->working_count = run->count;
        effective = run->destination + 3u;
        width = (size_t)(effective & 3u) + 1u;
        /* Little-endian SWL clears the start of effective's aligned word. It
         * intentionally overlaps prior word stores for many lengths. */
        TRY(write_zero(run, effective & ~UINT32_C(3), width, 1,
            0x800a3b8cu));
    }

    out->working_destination = run->destination;
    out->working_count = run->count;
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
