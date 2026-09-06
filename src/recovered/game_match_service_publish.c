#include "game_match_service_publish.h"

#include <string.h>

#define SOURCE_STATUS UINT32_C(0x800f9ffe)
#define SOURCE_PHASE UINT32_C(0x800fdb90)
#define PUBLISHED_STATUS UINT32_C(0x80015028)
#define PUBLISHED_PHASE UINT32_C(0x800170bc)

typedef struct Nba97GameMatchServicePublishRun {
    Nba97GameMatchServicePublishContext* context;
    Nba97GameMatchServicePublishProgress* out;
    Nba97GameMatchServicePublishMachine machine;
} Nba97GameMatchServicePublishRun;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameMatchServicePublishRun* run) {
    run->out->machine = run->machine;
}

static void stop(Nba97GameMatchServicePublishRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameMatchServicePublishWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameMatchServicePublishMachine* machine) {
    unsigned i;
    if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask !=
            0x0fu ||
        machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine->registers.gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameMatchServicePublishContext* context,
    Nba97GameMatchServicePublishProgress* out,
    Nba97GameMatchServicePublishRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count) ||
        (!context->access_journal && context->access_journal_capacity) ||
        !machine_valid(&context->machine))
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
    run->machine = context->machine;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

/* Enumerate byte-domain carry possibilities so ADDIU exposes every result byte
 * that is invariant across the concrete values represented by known_mask. */
static Nba97GameMatchServicePublishWord add_words(
    Nba97GameMatchServicePublishWord left,
    Nba97GameMatchServicePublishWord right) {
    Nba97GameMatchServicePublishWord result;
    unsigned carry_mask = 1u;
    unsigned byte;
    result.word = left.word + right.word;
    result.known_mask = 0;
    if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
        result.known_mask = 0x0fu;
        return result;
    }
    for (byte = 0; byte < 4; ++byte) {
        unsigned next_carry_mask = 0;
        unsigned first_output = 0;
        int first = 1;
        int invariant = 1;
        unsigned left_start = (left.known_mask & (1u << byte)) ?
            ((left.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned left_end = (left.known_mask & (1u << byte)) ?
            left_start : 255u;
        unsigned right_start = (right.known_mask & (1u << byte)) ?
            ((right.word >> (byte * 8u)) & 0xffu) : 0u;
        unsigned right_end = (right.known_mask & (1u << byte)) ?
            right_start : 255u;
        unsigned carry;
        for (carry = 0; carry <= 1; ++carry) {
            unsigned a;
            if (!(carry_mask & (1u << carry)))
                continue;
            for (a = left_start; a <= left_end; ++a) {
                unsigned b;
                for (b = right_start; b <= right_end; ++b) {
                    unsigned sum = a + b + carry;
                    unsigned output = sum & 0xffu;
                    next_carry_mask |= 1u << (sum >> 8u);
                    if (first) {
                        first_output = output;
                        first = 0;
                    } else if (output != first_output) {
                        invariant = 0;
                    }
                }
            }
        }
        if (invariant)
            result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
        carry_mask = next_carry_mask;
    }
    return result;
}

static Nba97GameMatchServicePublishWord add_constant(
    Nba97GameMatchServicePublishWord source, uint32_t constant) {
    Nba97GameMatchServicePublishWord value;
    set_known(&value, constant);
    return add_words(source, value);
}

static uint32_t width_mask(unsigned width) {
    return width == 4 ? UINT32_MAX :
        (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}

static int spend(Nba97GameMatchServicePublishRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameMatchServicePublishRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameMatchServicePublishWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameMatchServicePublishAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word & width_mask(width);
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask =
            (uint8_t)(value->known_mask & knowledge_mask(width));
        event->kind = kind;
    }
}

static int locate(Nba97GameMatchServicePublishRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    uint8_t** data, uint8_t** known) {
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

static int read_value(Nba97GameMatchServicePublishRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    Nba97GameMatchServicePublishWord* value) {
    Nba97GameMatchServicePublishWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, width, width, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_MATCH_SERVICE_PUBLISH_READ,
        pc, address, width, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameMatchServicePublishRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    const Nba97GameMatchServicePublishWord* value) {
    Nba97GameMatchServicePublishWord stored = *value;
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    stored.word &= width_mask(width);
    stored.known_mask =
        (uint8_t)(stored.known_mask & knowledge_mask(width));
    TRY(locate(run, address, width, width, pc, &data, &known));
    if (!known && stored.known_mask != knowledge_mask(width))
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(stored.word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_MATCH_SERVICE_PUBLISH_STORE,
        pc, address, width, &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int stack_address(Nba97GameMatchServicePublishRun* run,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GameMatchServicePublishWord value =
        add_constant(R(NBA97_MATCH_INITIALIZE_SP), offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameMatchServicePublishWord load_lhu(
    Nba97GameMatchServicePublishWord raw) {
    Nba97GameMatchServicePublishWord result;
    result.word = raw.word & 0xffffu;
    result.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
    return result;
}

static Nba97GameMatchServicePublishWord load_lh(
    Nba97GameMatchServicePublishWord raw) {
    Nba97GameMatchServicePublishWord result;
    uint32_t value = raw.word & 0xffffu;
    result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
    result.known_mask = (uint8_t)(raw.known_mask & 3u);
    if (raw.known_mask & 2u)
        result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
    return result;
}

static int invoke(Nba97GameMatchServicePublishRun* run) {
    Nba97GameMatchServicePublishEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8002de64));
    stop(run, UINT32_C(0x8002de5c), 0, UINT32_C(0x8002a264));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = UINT32_C(0x8002de5c);
    event.delay_slot_pc = UINT32_C(0x8002de60);
    event.entry = UINT32_C(0x8002a264);
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[
        NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264] + 1u;
    event.kind = NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264;
    event.argument_count = 0;
    publish(run);
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user, &run->context->memory,
        &event, &run->machine);
    run->out->child_return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
    run->out->child_return_v1 = R(NBA97_MATCH_INITIALIZE_V1);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!machine_valid(&run->machine))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[
        NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264];
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_service_publish(
    Nba97GameMatchServicePublishContext* context,
    Nba97GameMatchServicePublishProgress* out) {
    Nba97GameMatchServicePublishRun storage;
    Nba97GameMatchServicePublishRun* run = &storage;
    Nba97GameMatchServicePublishWord raw;
    uint32_t address;
    TRY(validate(context, out, run));

    /* 0x8002DE34..0x8002DE40: both source reads occur before SP or memory is
     * modified, so native aliases cannot retroactively change the values. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, SOURCE_STATUS, 2, UINT32_C(0x8002de38), &raw));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
    out->loaded_status = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(read_value(run, SOURCE_PHASE, 2, UINT32_C(0x8002de40), &raw));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lh(raw);
    out->loaded_phase = R(NBA97_MATCH_INITIALIZE_V1);

    /* 0x8002DE44..0x8002DE58: frame setup and publications retain source
     * order. Destination aliases may overwrite the saved return address. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(stack_address(run, 0x10u, UINT32_C(0x8002de48), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8002de48),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80010000));
    TRY(write_value(run, PUBLISHED_STATUS, 2, UINT32_C(0x8002de50),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80010000));
    TRY(write_value(run, PUBLISHED_PHASE, 4, UINT32_C(0x8002de58),
        &R(NBA97_MATCH_INITIALIZE_V1)));

    /* 0x8002DE5C/0x8002DE60: JAL 0x8002A264, with a NOP delay slot. */
    TRY(invoke(run));

    /* 0x8002DE64..0x8002DE70: restore via child-mutable live SP, advance SP,
     * then require a fully known JR target; the final delay slot is a NOP. */
    TRY(stack_address(run, 0x10u, UINT32_C(0x8002de64), &address));
    TRY(read_value(run, address, 4, UINT32_C(0x8002de64),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8002de6c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
