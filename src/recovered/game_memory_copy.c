#include "game_memory_copy.h"

#include <string.h>

typedef struct Nba97GameMemoryCopyWord {
    uint8_t data[4];
    uint8_t known[4];
} Nba97GameMemoryCopyWord;

typedef struct Nba97GameMemoryCopyRun {
    Nba97GameMemoryCopyContext* context;
    Nba97GameMemoryCopyProgress* out;
    uint32_t source;
    uint32_t destination;
    uint32_t count;
} Nba97GameMemoryCopyRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static int negative(uint32_t value) {
    return (value & UINT32_C(0x80000000)) != 0;
}

static int signed_less(uint32_t left, uint32_t right) {
    if ((left ^ right) & UINT32_C(0x80000000))
        return negative(left);
    return left < right;
}

static int signed_add(uint32_t left, uint32_t right, uint32_t* result) {
    uint32_t sum = left + right;
    *result = sum;
    return !(((left ^ sum) & (right ^ sum) & UINT32_C(0x80000000)) != 0);
}

static void sync(Nba97GameMemoryCopyRun* run) {
    run->out->working_source = run->source;
    run->out->working_destination = run->destination;
    run->out->working_count = run->count;
}

static void stop(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address) {
    sync(run);
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
}

static int locate(Nba97GameMemoryCopyRun* run, uint32_t address,
    size_t width, uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    ++run->out->accesses;
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

static int read_piece(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address, size_t width, uint32_t logical,
    Nba97GameMemoryCopyWord* value) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    TRY(locate(run, address, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        uint32_t offset = (address + (uint32_t)i) - logical;
        if (offset >= 4u)
            return NBA97_TEXT_ARGUMENT;
        value->data[offset] = data[i];
        value->known[offset] = known ? known[i] : 1;
    }
    ++run->out->reads;
    run->out->bytes_read += width;
    return NBA97_TEXT_COMPLETE;
}

static int write_piece(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address, size_t width, uint32_t logical,
    const Nba97GameMemoryCopyWord* value) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    TRY(locate(run, address, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        uint32_t offset = (address + (uint32_t)i) - logical;
        if (offset >= 4u)
            return NBA97_TEXT_ARGUMENT;
        if (!known && !value->known[offset])
            return NBA97_TEXT_UNKNOWN;
    }
    for (i = 0; i < width; ++i) {
        uint32_t offset = (address + (uint32_t)i) - logical;
        data[i] = value->data[offset];
        if (known)
            known[i] = value->known[offset];
    }
    ++run->out->stores;
    run->out->bytes_stored += width;
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address, Nba97GameMemoryCopyWord* value) {
    memset(value, 0, sizeof *value);
    return read_piece(run, pc, address, 4, address, value);
}

static int write_word(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address, const Nba97GameMemoryCopyWord* value) {
    return write_piece(run, pc, address, 4, address, value);
}

static int read_unaligned_word(Nba97GameMemoryCopyRun* run,
    uint32_t lwl_pc, uint32_t lwr_pc, uint32_t address,
    Nba97GameMemoryCopyWord* value) {
    uint32_t effective = address + 3u;
    size_t left_width = (size_t)(effective & 3u) + 1u;
    size_t right_width = 4u - (size_t)(address & 3u);
    memset(value, 0, sizeof *value);
    TRY(read_piece(run, lwl_pc, effective & ~UINT32_C(3), left_width,
        address, value));
    return read_piece(run, lwr_pc, address, right_width, address, value);
}

static int write_unaligned_word(Nba97GameMemoryCopyRun* run,
    uint32_t swl_pc, uint32_t swr_pc, uint32_t address,
    const Nba97GameMemoryCopyWord* value) {
    uint32_t effective = address + 3u;
    size_t left_width = (size_t)(effective & 3u) + 1u;
    size_t right_width = 4u - (size_t)(address & 3u);
    TRY(write_piece(run, swl_pc, effective & ~UINT32_C(3), left_width,
        address, value));
    return write_piece(run, swr_pc, address, right_width, address, value);
}

static int read_byte(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address, Nba97GameMemoryCopyWord* value) {
    memset(value, 0, sizeof *value);
    return read_piece(run, pc, address, 1, address, value);
}

static int write_byte(Nba97GameMemoryCopyRun* run, uint32_t pc,
    uint32_t address, const Nba97GameMemoryCopyWord* value) {
    return write_piece(run, pc, address, 1, address, value);
}

static int forward_aligned(Nba97GameMemoryCopyRun* run) {
    Nba97GameMemoryCopyWord values[8];
    unsigned half;
    unsigned i;

    run->count -= 0x40u; /* 0x800AA480. */
    sync(run);
    while (!negative(run->count)) {
        /* 0x800AA48C..508 snapshots each eight-word half before writing it.
         * A whole 64-byte host snapshot would change short-distance aliases. */
        for (half = 0; half < 2; ++half) {
            uint32_t offset = half * 0x20u;
            uint32_t read_pc = 0x800aa48cu + half * 0x40u;
            uint32_t write_pc = 0x800aa4acu + half * 0x40u;
            for (i = 0; i < 8; ++i)
                TRY(read_word(run, read_pc + i * 4u,
                    run->source + offset + i * 4u, &values[i]));
            for (i = 0; i < 8; ++i)
                TRY(write_word(run, write_pc + i * 4u,
                    run->destination + offset + i * 4u, &values[i]));
        }
        run->count -= 0x40u;
        run->source += 0x40u;
        run->destination += 0x40u;
        sync(run);
    }

    run->count += 0x30u; /* 0x800AA51C. */
    sync(run);
    while (!negative(run->count)) {
        for (i = 0; i < 4; ++i)
            TRY(read_word(run, 0x800aa528u + i * 4u,
                run->source + i * 4u, &values[i]));
        for (i = 0; i < 4; ++i)
            TRY(write_word(run, 0x800aa538u + i * 4u,
                run->destination + i * 4u, &values[i]));
        run->count -= 0x10u;
        run->source += 0x10u;
        run->destination += 0x10u;
        sync(run);
    }

    run->count += 0x0cu; /* 0x800AA558. */
    sync(run);
    while (!negative(run->count)) {
        TRY(read_word(run, 0x800aa564u, run->source, &values[0]));
        run->count -= 4u; /* Occurs before the SW at 0x800AA56C. */
        sync(run);
        TRY(write_word(run, 0x800aa56cu, run->destination, &values[0]));
        run->source += 4u;
        run->destination += 4u;
        sync(run);
    }

    run->count += 3u; /* 0x800AA57C. */
    sync(run);
    while (!negative(run->count)) {
        TRY(read_byte(run, 0x800aa588u, run->source, &values[0]));
        run->count -= 1u;
        sync(run);
        TRY(write_byte(run, 0x800aa590u, run->destination, &values[0]));
        run->source += 1u;
        run->destination += 1u;
        sync(run);
    }
    return NBA97_TEXT_COMPLETE;
}

static int forward_unaligned(Nba97GameMemoryCopyRun* run) {
    Nba97GameMemoryCopyWord values[4];
    unsigned i;
    run->count -= 0x10u; /* 0x800AA5A8. */
    sync(run);
    while (!negative(run->count)) {
        for (i = 0; i < 4; ++i)
            TRY(read_unaligned_word(run, 0x800aa5b4u + i * 8u,
                0x800aa5b8u + i * 8u, run->source + i * 4u,
                &values[i]));
        for (i = 0; i < 4; ++i)
            TRY(write_unaligned_word(run, 0x800aa5d4u + i * 8u,
                0x800aa5d8u + i * 8u, run->destination + i * 4u,
                &values[i]));
        run->count -= 0x10u;
        run->source += 0x10u;
        run->destination += 0x10u;
        sync(run);
    }

    run->count += 0x0cu; /* 0x800AA604. */
    sync(run);
    while (!negative(run->count)) {
        TRY(read_unaligned_word(run, 0x800aa610u, 0x800aa614u,
            run->source, &values[0]));
        run->count -= 4u;
        sync(run);
        TRY(write_unaligned_word(run, 0x800aa61cu, 0x800aa620u,
            run->destination, &values[0]));
        run->source += 4u;
        run->destination += 4u;
        sync(run);
    }

    run->count += 3u; /* 0x800AA630. */
    sync(run);
    while (!negative(run->count)) {
        TRY(read_byte(run, 0x800aa63cu, run->source, &values[0]));
        run->count -= 1u;
        sync(run);
        TRY(write_byte(run, 0x800aa644u, run->destination, &values[0]));
        run->source += 1u;
        run->destination += 1u;
        sync(run);
    }
    return NBA97_TEXT_COMPLETE;
}

static int backward_words_and_bytes(Nba97GameMemoryCopyRun* run) {
    Nba97GameMemoryCopyWord value;
    run->count += 0x0cu; /* 0x800AA6C0 or 0x800AA730. */
    sync(run);
    while (!negative(run->count)) {
        uint32_t source = run->source - 4u;
        uint32_t destination = run->destination - 4u;
        /* Even aligned backward tails use both partial-word instructions. */
        TRY(read_unaligned_word(run, 0x800aa73cu, 0x800aa740u,
            source, &value));
        run->count -= 4u;
        sync(run);
        TRY(write_unaligned_word(run, 0x800aa748u, 0x800aa74cu,
            destination, &value));
        run->source -= 4u;
        run->destination -= 4u;
        sync(run);
    }

    run->count += 3u; /* 0x800AA75C. */
    sync(run);
    while (!negative(run->count)) {
        uint32_t source = run->source - 1u;
        uint32_t destination = run->destination - 1u;
        TRY(read_byte(run, 0x800aa768u, source, &value));
        run->count -= 1u;
        sync(run);
        TRY(write_byte(run, 0x800aa770u, destination, &value));
        run->source -= 1u;
        run->destination -= 1u;
        sync(run);
    }
    return NBA97_TEXT_COMPLETE;
}

static int backward_aligned(Nba97GameMemoryCopyRun* run) {
    Nba97GameMemoryCopyWord values[4];
    unsigned i;
    run->count -= 0x10u; /* 0x800AA684. */
    sync(run);
    while (!negative(run->count)) {
        uint32_t source = run->source - 0x10u;
        uint32_t destination = run->destination - 0x10u;
        for (i = 0; i < 4; ++i)
            TRY(read_word(run, 0x800aa690u + i * 4u,
                source + i * 4u, &values[i]));
        for (i = 0; i < 4; ++i)
            TRY(write_word(run, 0x800aa6a0u + i * 4u,
                destination + i * 4u, &values[i]));
        run->source -= 0x10u;
        run->count -= 0x10u;
        run->destination -= 0x10u;
        sync(run);
    }
    return backward_words_and_bytes(run);
}

static int backward_unaligned(Nba97GameMemoryCopyRun* run) {
    Nba97GameMemoryCopyWord values[4];
    unsigned i;
    run->count -= 0x10u; /* 0x800AA6D4. */
    sync(run);
    while (!negative(run->count)) {
        uint32_t source = run->source - 0x10u;
        uint32_t destination = run->destination - 0x10u;
        for (i = 0; i < 4; ++i)
            TRY(read_unaligned_word(run, 0x800aa6e0u + i * 8u,
                0x800aa6e4u + i * 8u, source + i * 4u, &values[i]));
        for (i = 0; i < 4; ++i)
            TRY(write_unaligned_word(run, 0x800aa700u + i * 8u,
                0x800aa704u + i * 8u, destination + i * 4u,
                &values[i]));
        run->count -= 0x10u;
        run->source -= 0x10u;
        run->destination -= 0x10u;
        sync(run);
    }
    return backward_words_and_bytes(run);
}

static int validate(Nba97GameMemoryCopyContext* context,
    Nba97GameMemoryCopyProgress* out, Nba97GameMemoryCopyRun* run) {
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
    run->source = context->source;
    run->destination = context->destination;
    run->count = context->length;
    out->source = context->source;
    out->destination = context->destination;
    out->requested_length = context->length;
    /* 0x800AA470 is the delay slot of the initial branch and always runs. */
    out->return_v0 = context->source | context->destination;
    out->return_v0_known = 1;
    sync(run);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_memory_copy(Nba97GameMemoryCopyContext* context,
    Nba97GameMemoryCopyProgress* out) {
    Nba97GameMemoryCopyRun storage;
    Nba97GameMemoryCopyRun* run = &storage;
    uint32_t source_end;
    uint32_t destination_end;
    TRY(validate(context, out, run));

    /* Original direction choice uses signed SLT and trapping ADD, despite
     * addresses being represented as raw 32-bit source values in the port. */
    if (signed_less(run->source, run->destination)) {
        if (!signed_add(run->source, run->count, &source_end)) {
            stop(run, 0x800aa65cu, 0);
            return NBA97_GAME_MEMORY_COPY_ARITHMETIC_TRAP;
        }
        if (signed_less(run->destination, source_end)) {
            out->backward = 1;
            run->source = source_end; /* 0x800AA66C repeats the safe ADD. */
            sync(run);
            if (!signed_add(run->destination, run->count,
                    &destination_end)) {
                stop(run, 0x800aa670u, 0);
                return NBA97_GAME_MEMORY_COPY_ARITHMETIC_TRAP;
            }
            run->destination = destination_end;
            out->return_v0 = (run->source | run->destination) & 3u;
            out->unaligned = out->return_v0 != 0;
            sync(run);
            TRY(out->unaligned ? backward_unaligned(run) :
                backward_aligned(run));
            out->completed = 1;
            stop(run, 0, 0);
            return NBA97_TEXT_COMPLETE;
        }
    }

    out->return_v0 = (run->source | run->destination) & 3u;
    out->unaligned = out->return_v0 != 0;
    TRY(out->unaligned ? forward_unaligned(run) : forward_aligned(run));
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
