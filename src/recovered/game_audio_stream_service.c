#include "game_audio_stream_service.h"

#include <string.h>

#define HEADER_POINTER_ADDRESS UINT32_C(0x8010473c)

typedef struct Nba97GameAudioStreamServiceRun {
    Nba97GameAudioStreamServiceContext* context;
    Nba97GameAudioStreamServiceProgress* out;
    Nba97GameAudioStreamServiceRegisters registers;
} Nba97GameAudioStreamServiceRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameAudioStreamServiceRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameAudioStreamServiceRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int registers_valid(
    const Nba97GameAudioStreamServiceRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameAudioStreamServiceContext* context,
    Nba97GameAudioStreamServiceProgress* out,
    Nba97GameAudioStreamServiceRun* run) {
    size_t i;
    size_t j;
    if (!out)
        return NBA97_TEXT_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!context || (!context->memory.region && context->memory.count) ||
        (!context->access_journal && context->access_journal_capacity) ||
        !registers_valid(&context->registers))
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
    run->registers = context->registers;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static void set_known(Nba97GameAudioStreamServiceWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

/* ADDIU address arithmetic keeps exact byte-carry knownness. */
static Nba97GameAudioStreamServiceWord add_constant(
    Nba97GameAudioStreamServiceWord source, uint32_t addend) {
    const uint32_t original = source.word;
    const uint8_t original_known = source.known_mask;
    uint8_t result_known = 0;
    unsigned carry = 0;
    unsigned carry_known = 1;
    unsigned i;
    source.word += addend;
    for (i = 0; i < 4; ++i) {
        const unsigned byte_known = (original_known >> i) & 1u;
        const unsigned byte = (original >> (8u * i)) & 0xffu;
        const unsigned add_byte = (addend >> (8u * i)) & 0xffu;
        if (byte_known && carry_known)
            result_known = (uint8_t)(result_known | (1u << i));
        if (byte_known && carry_known) {
            carry = byte + add_byte + carry > 0xffu;
        } else if (!byte_known && carry_known && add_byte + carry == 0u) {
            carry = 0;
        } else if (!byte_known && carry_known && add_byte + carry == 0x100u) {
            carry = 1;
        } else if (byte_known && !carry_known && byte + add_byte != 0xffu) {
            carry = byte + add_byte > 0xffu;
            carry_known = 1;
        } else {
            carry_known = 0;
        }
    }
    source.known_mask = result_known;
    return source;
}

static int spend(Nba97GameAudioStreamServiceRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameAudioStreamServiceRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, const Nba97GameAudioStreamServiceWord* value) {
    const size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameAudioStreamServiceAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word;
        event->operation = run->out->operations;
        event->width = 4;
        event->known_mask = value->known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameAudioStreamServiceRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address, 0);
    TRY(spend(run));
    ++run->out->accesses;
    if (address & 3u)
        return NBA97_TEXT_ALIGNMENT_TRAP;
    for (i = 0; i < run->context->memory.count; ++i) {
        Nba97GameTextRegion* region = &run->context->memory.region[i];
        const uint64_t offset = (uint64_t)address - region->base;
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

static int read_word(Nba97GameAudioStreamServiceRun* run, uint32_t address,
    uint32_t pc, Nba97GameAudioStreamServiceWord* value) {
    Nba97GameAudioStreamServiceWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (8u * i);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_STREAM_SERVICE_READ, pc, address, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameAudioStreamServiceRun* run, uint32_t address,
    uint32_t pc, const Nba97GameAudioStreamServiceWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (8u * i));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_AUDIO_STREAM_SERVICE_STORE, pc, address, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int require_known(Nba97GameAudioStreamServiceRun* run,
    const Nba97GameAudioStreamServiceWord* value, uint32_t pc) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, value->word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int base_read(Nba97GameAudioStreamServiceRun* run, unsigned base,
    uint32_t offset, uint32_t pc, Nba97GameAudioStreamServiceWord* value) {
    Nba97GameAudioStreamServiceWord address = add_constant(R(base), offset);
    TRY(require_known(run, &address, pc));
    return read_word(run, address.word, pc, value);
}

static int base_write(Nba97GameAudioStreamServiceRun* run, unsigned base,
    uint32_t offset, uint32_t pc, const Nba97GameAudioStreamServiceWord* value) {
    Nba97GameAudioStreamServiceWord address = add_constant(R(base), offset);
    TRY(require_known(run, &address, pc));
    return write_word(run, address.word, pc, value);
}

static int equality(const Nba97GameAudioStreamServiceWord* left,
    const Nba97GameAudioStreamServiceWord* right, int* equal) {
    unsigned i;
    for (i = 0; i < 4; ++i) {
        const uint8_t bit = (uint8_t)(1u << i);
        if ((left->known_mask & bit) && (right->known_mask & bit) &&
            ((left->word >> (8u * i)) & 0xffu) !=
            ((right->word >> (8u * i)) & 0xffu)) {
            *equal = 0;
            return 1;
        }
    }
    if ((left->known_mask & right->known_mask) == 0x0fu) {
        *equal = left->word == right->word;
        return 1;
    }
    return 0;
}

static int invoke(Nba97GameAudioStreamServiceRun* run) {
    Nba97GameAudioStreamServiceEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800861cc));
    stop(run, UINT32_C(0x800861c4), 0, UINT32_C(0x800861e4));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = UINT32_C(0x800861c4);
    event.delay_slot_pc = UINT32_C(0x800861c8);
    event.entry = UINT32_C(0x800861e4);
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[
        NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4] + 1u;
    event.kind = NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4;
    event.argument_count = 0;
    publish(run);
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user, &run->context->memory,
        &event, &run->registers);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!registers_valid(&run->registers))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    ++run->out->call_count[NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4];
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_audio_stream_service(Nba97GameAudioStreamServiceContext* context,
    Nba97GameAudioStreamServiceProgress* out) {
    Nba97GameAudioStreamServiceRun storage;
    Nba97GameAudioStreamServiceRun* run = &storage;
    Nba97GameAudioStreamServiceWord one;
    int equal;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80086190..0x8008619C: allocate the 24-byte frame, save
     * ra/s8 in source order, and establish the new live s8 frame base. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_SP, 0x14u,
        UINT32_C(0x80086194), &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(base_write(run, NBA97_MATCH_INITIALIZE_SP, 0x10u,
        UINT32_C(0x80086198), &R(NBA97_MATCH_INITIALIZE_FP)));
    R(NBA97_MATCH_INITIALIZE_FP) = R(NBA97_MATCH_INITIALIZE_SP);

    /* 0x800861A0..0x800861B8: load the global header pointer, retain the
     * load-delay NOP, read live pointer+0x24, then publish ORI v0=1 before
     * the BNE and its NOP delay slot. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_word(run, HEADER_POINTER_ADDRESS, UINT32_C(0x800861a4),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    out->global_pointer = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_V0, 0x24u,
        UINT32_C(0x800861ac), &R(NBA97_MATCH_INITIALIZE_V1)));
    out->header_state = R(NBA97_MATCH_INITIALIZE_V1);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
    set_known(&one, 1u);
    publish(run);
    if (!equality(&R(NBA97_MATCH_INITIALIZE_V1), &one, &equal)) {
        stop(run, UINT32_C(0x800861b4), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }

    /* 0x800861BC..0x800861C8: exact state one takes J/NOP directly to the
     * epilogue; every proven unequal state takes the sole JAL/NOP child. */
    if (!equal)
        TRY(invoke(run));

    /* 0x800861CC..0x800861E0: child-live s8 replaces child sp, then saved
     * ra and s8 are reloaded in order before ADDIU and JR/NOP. */
    R(NBA97_MATCH_INITIALIZE_SP) = R(NBA97_MATCH_INITIALIZE_FP);
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_SP, 0x14u,
        UINT32_C(0x800861d0), &R(NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(base_read(run, NBA97_MATCH_INITIALIZE_SP, 0x10u,
        UINT32_C(0x800861d4), &R(NBA97_MATCH_INITIALIZE_FP)));
    out->restored_s8 = R(NBA97_MATCH_INITIALIZE_FP);
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
    out->returned_value = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(require_known(run, &R(NBA97_MATCH_INITIALIZE_RA),
        UINT32_C(0x800861dc)));
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
