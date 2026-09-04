#include "game_move_image.h"

#include <string.h>

#define MOVE_IMAGE_TEXT UINT32_C(0x8002831c)
#define DIAGNOSTIC_ENTRY UINT32_C(0x80099560)
#define DRIVER_TABLE_ADDRESS UINT32_C(0x800c55b8)
#define PACKET_ADDRESS UINT32_C(0x800c5668)
#define PACKET_SOURCE_ADDRESS UINT32_C(0x800c5670)
#define PACKET_DESTINATION_ADDRESS UINT32_C(0x800c5674)
#define PACKET_EXTENT_ADDRESS UINT32_C(0x800c5678)

typedef struct Nba97GameMoveImageRun {
    Nba97GameMoveImageContext* context;
    Nba97GameMoveImageProgress* out;
    uint32_t sp;
    uint32_t s[3];
} Nba97GameMoveImageRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameMoveImageRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameMoveImageRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate(Nba97GameMoveImageRun* run, uint32_t address,
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

static int read_required(Nba97GameMoveImageRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc, uint32_t* value) {
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

static int write_word(Nba97GameMoveImageRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    size_t i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int invoke(Nba97GameMoveImageRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
    Nba97GameMoveImageValue* value) {
    Nba97GameMoveImageEvent event;
    int result;
    unsigned i;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    if (kind == NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH && (entry & 3u))
        return NBA97_TEXT_ALIGNMENT_TRAP;
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.entry = entry;
    event.argument[0] = a0;
    event.argument[1] = a1;
    event.argument[2] = a2;
    event.argument[3] = a3;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    for (i = 0; i < 3; ++i)
        event.saved_register[i] = run->s[i];
    event.return_address = pc + 8u;
    event.kind = kind;
    event.argument_count = argument_count;
    value->word = 0;
    value->known = 0;
    result = run->context->io(run->context->user,
        &run->context->memory, &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    /* 0x80099560's return is overwritten by the following LH. Only the live
     * GPU dispatch result needs representable knownness. */
    if (kind == NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH && value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

static int32_t signed_half(uint32_t value) {
    value &= 0xffffu;
    return value < 0x8000u ? (int32_t)value : (int32_t)value - 0x10000;
}

static int validate(Nba97GameMoveImageContext* context,
    Nba97GameMoveImageProgress* out, Nba97GameMoveImageRun* run) {
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
    for (i = 0; i < 3; ++i)
        run->s[i] = context->saved_register[i];
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->rectangle_address = context->rectangle_address;
    out->requested_destination_x = context->destination_x;
    out->requested_destination_y = context->destination_y;
    out->packet_address = PACKET_ADDRESS;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_move_image(Nba97GameMoveImageContext* context,
    Nba97GameMoveImageProgress* out) {
    Nba97GameMoveImageRun storage;
    Nba97GameMoveImageRun* run = &storage;
    Nba97GameMoveImageValue value;
    uint32_t width;
    uint32_t height;
    uint32_t table;
    unsigned i;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800997E4 prologue. The unusual s0/s2/s1 save order and all
     * four live epilogue reloads stay observable to aliased child callbacks. */
    TRY(write_word(run, run->sp + 0x10u, 0x800997e8u, run->s[0]));
    run->s[0] = context->rectangle_address;
    TRY(write_word(run, run->sp + 0x18u, 0x800997f0u, run->s[2]));
    run->s[2] = context->destination_x;
    TRY(write_word(run, run->sp + 0x14u, 0x800997f8u, run->s[1]));
    run->s[1] = context->destination_y;
    TRY(write_word(run, run->sp + 0x1cu, 0x80099808u,
        context->return_address));

    /* The SDK diagnostic boundary is unconditional and precedes both source
     * zero-extent checks. Its raw v0 is intentionally discarded. */
    out->diagnostic_called = 1;
    TRY(invoke(run, 0x8009980cu, DIAGNOSTIC_ENTRY,
        NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC, 2,
        MOVE_IMAGE_TEXT, run->s[0], 0, 0, &value));

    TRY(read_required(run, run->s[0] + 4u, 2, 2,
        0x80099814u, &width));
    out->width_read = 1;
    out->signed_width = signed_half(width);
    out->return_v0 = UINT32_MAX;
    out->return_v0_known = 1;
    if ((width & 0xffffu) == 0) {
        out->zero_extent_return = 1;
    } else {
        TRY(read_required(run, run->s[0] + 6u, 2, 2,
            0x80099824u, &height));
        out->height_read = 1;
        out->signed_height = signed_half(height);
        if ((height & 0xffffu) == 0) {
            out->zero_extent_return = 1;
        } else {
            out->destination_coordinate_word =
                (run->s[1] << 16u) | (run->s[2] & 0xffffu);
            TRY(read_required(run, run->s[0], 4, 4,
                0x8009984cu, &out->source_coordinate_word));
            TRY(read_required(run, DRIVER_TABLE_ADDRESS, 4, 4,
                0x80099854u, &table));
            out->driver_table = table;
            /* Only the final three words of the shared five-word packet are
             * written. 0x800C5668..6F deliberately retain their live bytes. */
            TRY(write_word(run, PACKET_DESTINATION_ADDRESS,
                0x80099860u, out->destination_coordinate_word));
            TRY(write_word(run, PACKET_SOURCE_ADDRESS,
                0x80099864u, out->source_coordinate_word));
            TRY(read_required(run, run->s[0] + 4u, 4, 4,
                0x80099868u, &out->extent_word));
            TRY(write_word(run, PACKET_EXTENT_ADDRESS,
                0x80099874u, out->extent_word));
            TRY(read_required(run, table + 0x18u, 4, 4,
                0x80099878u, &out->dispatch_context));
            TRY(read_required(run, table + 8u, 4, 4,
                0x8009987cu, &out->dispatch_entry));
            out->gpu_dispatched = 1;
            TRY(invoke(run, 0x80099884u, out->dispatch_entry,
                NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH, 4,
                out->dispatch_context, PACKET_ADDRESS, 0x14u, 0,
                &value));
            out->return_v0 = value.word;
            out->return_v0_known = value.known;
        }
    }

    /* 0x8009988C..0x800998A4 reloads the caller state from live mapped
     * memory even on the two early -1 paths. */
    TRY(read_required(run, run->sp + 0x1cu, 4, 4,
        0x8009988cu, &out->restored_return_address));
    TRY(read_required(run, run->sp + 0x18u, 4, 4,
        0x80099890u, &out->restored_saved_register[2]));
    TRY(read_required(run, run->sp + 0x14u, 4, 4,
        0x80099894u, &out->restored_saved_register[1]));
    TRY(read_required(run, run->sp + 0x10u, 4, 4,
        0x80099898u, &out->restored_saved_register[0]));
    for (i = 0; i < 3; ++i)
        run->s[i] = out->restored_saved_register[i];
    run->sp += 0x20u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
