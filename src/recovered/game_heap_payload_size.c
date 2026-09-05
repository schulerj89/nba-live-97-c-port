#include "game_heap_payload_size.h"

#include <string.h>

#define FIND_DESCRIPTOR_ENTRY UINT32_C(0x80090618)

typedef struct Nba97GameHeapPayloadSizeRun {
    Nba97GameHeapPayloadSizeContext* context;
    Nba97GameHeapPayloadSizeProgress* out;
    uint32_t sp;
} Nba97GameHeapPayloadSizeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void stop(Nba97GameHeapPayloadSizeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
}

static int spend(Nba97GameHeapPayloadSizeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int locate_word(Nba97GameHeapPayloadSizeRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    unsigned j;
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

static int write_word(Nba97GameHeapPayloadSizeRun* run, uint32_t address,
    uint32_t pc, uint32_t value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate_word(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (i * 8u));
        if (known)
            known[i] = 1;
    }
    ++run->out->stores;
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameHeapPayloadSizeRun* run, uint32_t address,
    uint32_t pc, uint32_t* value) {
    uint8_t* data;
    uint8_t* known;
    uint32_t result = 0;
    unsigned i;
    TRY(locate_word(run, address, pc, &data, &known));
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

static int validate(Nba97GameHeapPayloadSizeContext* context,
    Nba97GameHeapPayloadSizeProgress* out,
    Nba97GameHeapPayloadSizeRun* run) {
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
    out->frame_stack_pointer = run->sp;
    out->stack_pointer = run->sp;
    out->global_pointer = context->global_pointer;
    out->payload = context->payload;
    return NBA97_TEXT_COMPLETE;
}

static int find_descriptor(Nba97GameHeapPayloadSizeRun* run,
    Nba97GameHeapPayloadSizeValue* value) {
    Nba97GameHeapPayloadSizeEvent event;
    int result;
    stop(run, 0x80090d68u, 0, FIND_DESCRIPTOR_ENTRY);
    TRY(spend(run));
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    memset(&event, 0, sizeof event);
    event.pc = 0x80090d68u;
    event.entry = FIND_DESCRIPTOR_ENTRY;
    event.argument[0] = run->context->payload;
    event.stack_pointer = run->sp;
    event.global_pointer = run->context->global_pointer;
    event.return_address = 0x80090d70u;
    event.kind = NBA97_GAME_HEAP_PAYLOAD_SIZE_FIND_DESCRIPTOR;
    event.argument_count = 1;
    value->word = 0;
    value->known = 0;
    result = run->context->io(run->context->user,
        &run->context->memory, &event, value);
    if (result != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (value->known > 1)
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->descriptor_lookup_calls;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_heap_payload_size(Nba97GameHeapPayloadSizeContext* context,
    Nba97GameHeapPayloadSizeProgress* out) {
    Nba97GameHeapPayloadSizeRun storage;
    Nba97GameHeapPayloadSizeRun* run = &storage;
    Nba97GameHeapPayloadSizeValue descriptor;
    uint32_t address;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80090D60..0x80090D68: the wrapper saves only ra and
     * forwards the incoming payload unchanged to the heap-list lookup. */
    TRY(write_word(run, run->sp + 0x10u, 0x80090d64u,
        context->return_address));
    TRY(find_descriptor(run, &descriptor));
    out->descriptor = descriptor.word;
    out->descriptor_known = descriptor.known;
    if (!descriptor.known) {
        stop(run, 0x80090d70u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }

    /* Original 0x80090D70 has no NULL guard. Descriptor zero therefore
     * reads address 0x14; pointer addition also wraps exactly at 32 bits. */
    address = descriptor.word + 0x14u;
    TRY(read_word(run, address, 0x80090d70u, &out->requested_size));
    out->return_v0 = out->requested_size;
    out->return_v0_known = 1;

    /* The size load precedes this live 0x80090D74 ra reload. A callback or
     * descriptor alias may change either mapped value before it is read. */
    TRY(read_word(run, run->sp + 0x10u, 0x80090d74u,
        &out->restored_return_address));
    run->sp += 0x18u;
    out->stack_pointer = run->sp;
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
