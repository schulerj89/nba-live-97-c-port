#include "game_audio_initialize.h"

#include <string.h>

#define BANK_HEADER_ADDRESS UINT32_C(0x8001502c)
#define BANK_HEADER_NAME_ADDRESS UINT32_C(0x800247bc)
#define BANK_BODY_NAME_ADDRESS UINT32_C(0x800247c8)
#define BANK_UPLOAD_STATE_ADDRESS UINT32_C(0x80021d6c)
#define VOLUME_SETTING_ADDRESS UINT32_C(0x80021d7c)
#define VOLUME_RESULT_ADDRESS UINT32_C(0x80021ee0)

typedef struct Nba97GameAudioInitializeRun {
    Nba97GameAudioInitializeContext* context;
    Nba97GameAudioInitializeProgress* out;
    Nba97GameAudioInitializeRegisters registers;
} Nba97GameAudioInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameAudioInitializeRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameAudioInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameAudioInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameAudioInitializeRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameAudioInitializeWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameAudioInitializeAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word;
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask = value->known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameAudioInitializeRun* run, uint32_t address,
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

static int read_word(Nba97GameAudioInitializeRun* run, uint32_t address,
    uint32_t pc, Nba97GameAudioInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameAudioInitializeWord loaded;
    unsigned i;
    loaded.word = 0;
    loaded.known_mask = 0;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_INITIALIZE_READ, pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int read_byte(Nba97GameAudioInitializeRun* run, uint32_t address,
    uint32_t pc, Nba97GameAudioInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameAudioInitializeWord loaded;
    TRY(locate(run, address, 1, 1, pc, &data, &known));
    loaded.word = data[0];
    loaded.known_mask = (uint8_t)((!known || known[0]) ? 1u : 0u);
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_AUDIO_INITIALIZE_READ, pc, address, 1, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameAudioInitializeRun* run, uint32_t address,
    uint32_t pc, const Nba97GameAudioInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_AUDIO_INITIALIZE_STORE, pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(
    const Nba97GameAudioInitializeRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameAudioInitializeContext* context,
    Nba97GameAudioInitializeProgress* out,
    Nba97GameAudioInitializeRun* run) {
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

static int require_known(Nba97GameAudioInitializeRun* run,
    unsigned index, uint32_t pc) {
    if (run->registers.gpr[index].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

enum Nba97GameAudioInitializeDelayKind {
    NBA97_GAME_AUDIO_INITIALIZE_DELAY_NOP = 0,
    NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT,
    NBA97_GAME_AUDIO_INITIALIZE_DELAY_MOVE
};

static int invoke(Nba97GameAudioInitializeRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    enum Nba97GameAudioInitializeDelayKind delay_kind,
    unsigned delay_destination, unsigned delay_source, uint32_t delay_value) {
    Nba97GameAudioInitializeEvent event;
    int accepted;
    /* JAL writes ra before its delay instruction. Native refusal or budget
     * exhaustion keeps both source instructions observable. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = pc + 8u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    if (delay_kind == NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT) {
        run->registers.gpr[delay_destination].word = delay_value;
        run->registers.gpr[delay_destination].known_mask = 0x0fu;
    } else if (delay_kind == NBA97_GAME_AUDIO_INITIALIZE_DELAY_MOVE) {
        run->registers.gpr[delay_destination] =
            run->registers.gpr[delay_source];
    }
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.kind = kind;
    event.argument_count = argument_count;
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
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_audio_initialize(Nba97GameAudioInitializeContext* context,
    Nba97GameAudioInitializeProgress* out) {
    Nba97GameAudioInitializeRun storage;
    Nba97GameAudioInitializeRun* run = &storage;
    Nba97GameAudioInitializeWord value;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80029114..0x80029130: read the previous bank before forming
     * the frame. The branch delay always saves incoming s0 before the optional
     * release decision can be resolved. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GameAudioInitializeWord){UINT32_C(0x80010000), 0x0f};
    TRY(read_word(run, BANK_HEADER_ADDRESS, 0x80029118u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_A0]));
    out->old_bank_header = run->registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, 0x8002911cu));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word -= 0x18u;
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    TRY(write_word(run, out->frame_stack_pointer + 0x14u, 0x80029120u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    TRY(write_word(run, out->frame_stack_pointer + 0x10u, 0x80029128u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_A0, 0x80029124u));
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word != 0)
        TRY(invoke(run, 0x8002912cu, 0x80090698u,
            NBA97_GAME_AUDIO_INITIALIZE_HEAP_RELEASE, 1,
            NBA97_GAME_AUDIO_INITIALIZE_DELAY_NOP, 0, 0, 0));

    /* GAMEONLY 0x80029134..0x80029158: load the header and body names. The
     * first raw v0 is published before the second loader call. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GameAudioInitializeWord){BANK_HEADER_NAME_ADDRESS, 0x0f};
    TRY(invoke(run, 0x8002913cu, 0x80029bfcu,
        NBA97_GAME_AUDIO_INITIALIZE_RESOURCE_LOAD, 2,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A1, 0, 0));
    out->new_bank_header = run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GameAudioInitializeWord){BANK_BODY_NAME_ADDRESS, 0x0f};
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
        (Nba97GameAudioInitializeWord){UINT32_C(0x80010000), 0x0f};
    TRY(write_word(run, BANK_HEADER_ADDRESS, 0x80029150u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    TRY(invoke(run, 0x80029154u, 0x80029bfcu,
        NBA97_GAME_AUDIO_INITIALIZE_RESOURCE_LOAD, 2,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A1, 0, 0));
    out->bank_body = run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];

    /* GAMEONLY 0x8002915C..0x8002918C: the first service call captures the
     * body handle into s0 in its delay slot; later callbacks see live changes. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GameAudioInitializeWord){UINT32_MAX, 0x0f};
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        (Nba97GameAudioInitializeWord){0, 0x0f};
    TRY(invoke(run, 0x80029164u, 0x8008f4f0u,
        NBA97_GAME_AUDIO_INITIALIZE_F4F0, 2,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_MOVE,
        NBA97_MATCH_INITIALIZE_S0, NBA97_MATCH_INITIALIZE_V0, 0));
    TRY(invoke(run, 0x8002916cu, 0x800adb48u,
        NBA97_GAME_AUDIO_INITIALIZE_ADB48, 0,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_NOP, 0, 0, 0));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GameAudioInitializeWord){4, 0x0f};
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        (Nba97GameAudioInitializeWord){11000, 0x0f};
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A2] =
        (Nba97GameAudioInitializeWord){0, 0x0f};
    TRY(invoke(run, 0x80029180u, 0x8008cdc0u,
        NBA97_GAME_AUDIO_INITIALIZE_CDC0, 4,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A3, 0, 0));
    TRY(invoke(run, 0x80029188u, 0x8008cc28u,
        NBA97_GAME_AUDIO_INITIALIZE_CC28, 0,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_NOP, 0, 0, 0));

    /* GAMEONLY 0x80029190..0x800291B4: reload the published header live,
     * upload with live s0, release that body, then set master volume. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        (Nba97GameAudioInitializeWord){UINT32_C(0x80010000), 0x0f};
    TRY(read_word(run, BANK_HEADER_ADDRESS, 0x80029194u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_A1]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        (Nba97GameAudioInitializeWord){BANK_UPLOAD_STATE_ADDRESS, 0x0f};
    TRY(invoke(run, 0x800291a0u, 0x800ad360u,
        NBA97_GAME_AUDIO_INITIALIZE_BANK_UPLOAD, 3,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_MOVE,
        NBA97_MATCH_INITIALIZE_A2, NBA97_MATCH_INITIALIZE_S0, 0));
    TRY(invoke(run, 0x800291a8u, 0x80090698u,
        NBA97_GAME_AUDIO_INITIALIZE_HEAP_RELEASE, 1,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_MOVE,
        NBA97_MATCH_INITIALIZE_A0, NBA97_MATCH_INITIALIZE_S0, 0));
    TRY(invoke(run, 0x800291b0u, 0x800aca08u,
        NBA97_GAME_AUDIO_INITIALIZE_MASTER_VOLUME, 1,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A0, 0, 127));

    /* GAMEONLY 0x800291B8..0x800291E8: LBU is followed by its explicit load
     * delay, then unsigned setting*15 is clamped before channel -1. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] =
        (Nba97GameAudioInitializeWord){UINT32_C(0x80020000), 0x0f};
    TRY(read_byte(run, VOLUME_SETTING_ADDRESS, 0x800291bcu, &value));
    out->volume_setting = value;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1].word = value.word;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask =
        (uint8_t)(value.known_mask | 0x0eu);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V1].word << 4u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask =
        value.known_mask ? 0x0fu : 0x0cu;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word -
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V1].word;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask =
        value.known_mask ? 0x0fu : 0;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word =
        (uint32_t)((int32_t)run->registers
            .gpr[NBA97_MATCH_INITIALIZE_A0].word < 128);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask =
        value.known_mask ? 0x0fu : 0x0eu;
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask != 0x0fu) {
        stop(run, 0x800291d0u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word = 127;
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 0x0fu;
    }
    out->scaled_volume = run->registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    TRY(invoke(run, 0x800291dcu, 0x80088e84u,
        NBA97_GAME_AUDIO_INITIALIZE_CHANNEL_VOLUME, 2,
        NBA97_GAME_AUDIO_INITIALIZE_DELAY_CONSTANT,
        NBA97_MATCH_INITIALIZE_A1, 0, UINT32_MAX));
    out->raw_volume_return = run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] =
        (Nba97GameAudioInitializeWord){UINT32_C(0x80020000), 0x0f};
    TRY(write_word(run, VOLUME_RESULT_ADDRESS, 0x800291e8u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));

    /* GAMEONLY 0x800291EC..0x800291FC: use the callback-mutated live sp for
     * ra then s0 reloads, restore that sp, and require raw ra only at JR. */
    TRY(require_known(run, NBA97_MATCH_INITIALIZE_SP, 0x800291ecu));
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x14u,
        0x800291ecu, &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x10u,
        0x800291f0u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    out->restored_s0 = run->registers.gpr[NBA97_MATCH_INITIALIZE_S0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 0x18u;
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 0x0fu) {
        stop(run, 0x800291f8u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
