#include "game_speech_initialize.h"

#include <string.h>

#define LANGUAGE_ADDRESS UINT32_C(0x80015018)
#define AUX_EVENT_POINTER UINT32_C(0x800fe9c8)
#define AUX_NONEVENT_POINTER UINT32_C(0x800febdc)
#define HOME_TEAM_ADDRESS UINT32_C(0x8001edf4)
#define AWAY_TEAM_ADDRESS UINT32_C(0x8001eeb8)
#define AWAY_ROSTER_TABLE UINT32_C(0x80020bbc)
#define RECORD_BASE UINT32_C(0x80102fe0)
#define ALLOCATION_POINTER UINT32_C(0x800feabc)

typedef struct Nba97GameSpeechInitializeRun {
    Nba97GameSpeechInitializeContext* context;
    Nba97GameSpeechInitializeProgress* out;
    Nba97GameSpeechInitializeRegisters registers;
} Nba97GameSpeechInitializeRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static Nba97GameSpeechInitializeWord word(uint32_t value) {
    Nba97GameSpeechInitializeWord result = {value, 0x0f};
    return result;
}

static void publish(Nba97GameSpeechInitializeRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameSpeechInitializeRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameSpeechInitializeRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameSpeechInitializeRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameSpeechInitializeWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameSpeechInitializeAccess* event =
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

static int locate(Nba97GameSpeechInitializeRun* run, uint32_t address,
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

static int read_value(Nba97GameSpeechInitializeRun* run, uint32_t address,
    uint32_t pc, uint8_t width, uint8_t alignment,
    Nba97GameSpeechInitializeWord* value) {
    Nba97GameSpeechInitializeWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    for (i = 0; i < width; ++i) {
        loaded.word |= (uint32_t)data[i] << (8u * i);
        if (!known || known[i])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (uint8_t)(1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_SPEECH_INITIALIZE_READ,
        pc, address, width, value);
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameSpeechInitializeRun* run, uint32_t address,
    uint32_t pc, Nba97GameSpeechInitializeWord* value) {
    return read_value(run, address, pc, 4, 4, value);
}

static int read_half(Nba97GameSpeechInitializeRun* run, uint32_t address,
    uint32_t pc, int is_signed, Nba97GameSpeechInitializeWord* value) {
    Nba97GameSpeechInitializeWord loaded;
    TRY(read_value(run, address, pc, 2, 2, &loaded));
    if (is_signed) {
        loaded.word = (uint32_t)(int32_t)(int16_t)loaded.word;
        if (loaded.known_mask & 2u)
            loaded.known_mask = (uint8_t)(loaded.known_mask | 0x0cu);
    } else {
        loaded.word &= UINT32_C(0x0000ffff);
        loaded.known_mask = (uint8_t)(loaded.known_mask | 0x0cu);
    }
    *value = loaded;
    return NBA97_TEXT_COMPLETE;
}

static int read_byte_signed(Nba97GameSpeechInitializeRun* run,
    uint32_t address, uint32_t pc, Nba97GameSpeechInitializeWord* value) {
    Nba97GameSpeechInitializeWord loaded;
    TRY(read_value(run, address, pc, 1, 1, &loaded));
    loaded.word = (uint32_t)(int32_t)(int8_t)loaded.word;
    if (loaded.known_mask & 1u)
        loaded.known_mask = 0x0f;
    *value = loaded;
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameSpeechInitializeRun* run, uint32_t address,
    uint32_t pc, const Nba97GameSpeechInitializeWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, 4, 4, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (8u * i));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_SPEECH_INITIALIZE_STORE,
        pc, address, 4, value);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(
    const Nba97GameSpeechInitializeRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameSpeechInitializeContext* context,
    Nba97GameSpeechInitializeProgress* out,
    Nba97GameSpeechInitializeRun* run) {
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

static int require_register(Nba97GameSpeechInitializeRun* run,
    unsigned index, uint32_t pc) {
    if (run->registers.gpr[index].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameSpeechInitializeWord add_words(
    Nba97GameSpeechInitializeWord left,
    Nba97GameSpeechInitializeWord right) {
    Nba97GameSpeechInitializeWord result = {left.word + right.word, 0};
    unsigned observed_carry = 0;
    unsigned carry_min = 0;
    unsigned carry_max = 0;
    unsigned i;
    for (i = 0; i < 4; ++i) {
        unsigned lk = (left.known_mask >> i) & 1u;
        unsigned rk = (right.known_mask >> i) & 1u;
        unsigned lb = (left.word >> (8u * i)) & 0xffu;
        unsigned rb = (right.word >> (8u * i)) & 0xffu;
        unsigned minimum = (lk ? lb : 0) + (rk ? rb : 0) + carry_min;
        unsigned maximum = (lk ? lb : 0xffu) +
            (rk ? rb : 0xffu) + carry_max;
        if (minimum == maximum)
            result.known_mask =
                (uint8_t)(result.known_mask | (uint8_t)(1u << i));
        observed_carry = lb + rb + observed_carry > 0xffu;
        carry_min = minimum > 0xffu;
        carry_max = maximum > 0xffu;
    }
    return result;
}

static Nba97GameSpeechInitializeWord add_immediate(
    Nba97GameSpeechInitializeWord value, uint32_t immediate) {
    return add_words(value, word(immediate));
}

/* Equality returns one/zero when decided and -1 when unknown bytes can alter
 * the result. */
static int equal_state(const Nba97GameSpeechInitializeWord* left,
    const Nba97GameSpeechInitializeWord* right) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((left->known_mask & right->known_mask & (1u << i)) &&
            ((left->word ^ right->word) & (UINT32_C(0xff) << (8u * i))))
            return 0;
    return left->known_mask == 0x0fu && right->known_mask == 0x0fu ? 1 : -1;
}

static int zero_state(const Nba97GameSpeechInitializeWord* value) {
    Nba97GameSpeechInitializeWord zero = {0, 0x0f};
    return equal_state(value, &zero);
}

static Nba97GameSpeechInitializeWord unsigned_less(
    Nba97GameSpeechInitializeWord left,
    Nba97GameSpeechInitializeWord right) {
    Nba97GameSpeechInitializeWord result = {
        (uint32_t)(left.word < right.word), 0x0e};
    uint32_t left_min = left.word;
    uint32_t left_max = left.word;
    uint32_t right_min = right.word;
    uint32_t right_max = right.word;
    unsigned i;
    for (i = 0; i < 4; ++i) {
        uint32_t mask = UINT32_C(0xff) << (8u * i);
        if (!(left.known_mask & (1u << i))) {
            left_min &= ~mask;
            left_max |= mask;
        }
        if (!(right.known_mask & (1u << i))) {
            right_min &= ~mask;
            right_max |= mask;
        }
    }
    if (left_max < right_min) {
        result.word = 1;
        result.known_mask = 0x0f;
    } else if (left_min >= right_max) {
        result.word = 0;
        result.known_mask = 0x0f;
    }
    return result;
}

static Nba97GameSpeechInitializeWord signed_less_immediate(
    Nba97GameSpeechInitializeWord value, int32_t immediate) {
    Nba97GameSpeechInitializeWord result = {
        (uint32_t)((int32_t)value.word < immediate), 0x0e};
    int64_t minimum;
    int64_t maximum;
    uint32_t unsigned_min = value.word;
    uint32_t unsigned_max = value.word;
    unsigned i;
    if (!(value.known_mask & 8u))
        return result;
    for (i = 0; i < 3; ++i) {
        uint32_t mask = UINT32_C(0xff) << (8u * i);
        if (!(value.known_mask & (1u << i))) {
            unsigned_min &= ~mask;
            unsigned_max |= mask;
        }
    }
    if (value.word & UINT32_C(0x80000000)) {
        minimum = (int32_t)unsigned_min;
        maximum = (int32_t)unsigned_max;
    } else {
        minimum = unsigned_min;
        maximum = unsigned_max;
    }
    if (maximum < immediate) {
        result.word = 1;
        result.known_mask = 0x0f;
    } else if (minimum >= immediate) {
        result.word = 0;
        result.known_mask = 0x0f;
    }
    return result;
}

static size_t next_invocation(const Nba97GameSpeechInitializeRun* run,
    uint8_t kind) {
    return run->out->call_count[kind] + 1u;
}

static int invoke(Nba97GameSpeechInitializeRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count) {
    Nba97GameSpeechInitializeEvent event;
    int accepted;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA] = word(pc + 8u);
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = next_invocation(run, kind);
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
    ++run->out->call_count[kind];
    return NBA97_TEXT_COMPLETE;
}

static int decide_branch(Nba97GameSpeechInitializeRun* run,
    const Nba97GameSpeechInitializeWord* condition, uint32_t pc) {
    if (condition->known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return -1;
    }
    return condition->word != 0;
}

static int load_resource(Nba97GameSpeechInitializeRun* run,
    uint32_t pc, uint32_t filename, uint32_t flags) {
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(filename);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(flags);
    return invoke(run, pc, UINT32_C(0x80029bfc),
        NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC, 2);
}

static int lookup(Nba97GameSpeechInitializeRun* run, uint32_t pc,
    uint32_t category, Nba97GameSpeechInitializeWord identifier,
    Nba97GameSpeechInitializeWord destination) {
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(category);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A2] = identifier;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A3] = destination;
    return invoke(run, pc, UINT32_C(0x8007fc08),
        NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08, 4);
}

int nba97_game_speech_initialize(Nba97GameSpeechInitializeContext* context,
    Nba97GameSpeechInitializeProgress* out) {
    Nba97GameSpeechInitializeRun storage;
    Nba97GameSpeechInitializeRun* run = &storage;
    Nba97GameSpeechInitializeWord value;
    Nba97GameSpeechInitializeWord condition;
    int branch;
    unsigned i;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8007FD40..0x8007FD8C: the first language read precedes the
     * wrapping frame ADDIU. The BEQ delay always stores entry s0. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] = word(0x80010000u);
    TRY(read_word(run, LANGUAGE_ADDRESS, 0x8007fd44u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]));
    out->first_language = run->registers.gpr[NBA97_MATCH_INITIALIZE_V1];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_immediate(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP], 0xffffffc8u);
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = word(1);
    TRY(require_register(run, NBA97_MATCH_INITIALIZE_SP, 0x8007fd50u));
    TRY(write_word(run, out->frame_stack_pointer + 0x34u, 0x8007fd50u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    for (i = 0; i < 8; ++i) {
        static const unsigned saved[8] = {
            NBA97_MATCH_INITIALIZE_FP,
            NBA97_MATCH_INITIALIZE_S0 + 7,
            NBA97_MATCH_INITIALIZE_S0 + 6,
            NBA97_MATCH_INITIALIZE_S0 + 5,
            NBA97_MATCH_INITIALIZE_S0 + 4,
            NBA97_MATCH_INITIALIZE_S0 + 3,
            NBA97_MATCH_INITIALIZE_S0 + 2,
            NBA97_MATCH_INITIALIZE_S0 + 1
        };
        unsigned reg = saved[i];
        uint32_t offset = 0x30u - i * 4u;
        TRY(write_word(run, out->frame_stack_pointer + offset,
            0x8007fd54u + i * 4u, &run->registers.gpr[reg]));
    }
    TRY(write_word(run, out->frame_stack_pointer + 0x10u, 0x8007fd78u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    branch = equal_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_V1],
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]);
    if (branch < 0) {
        stop(run, 0x8007fd74u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    if (branch) {
        TRY(load_resource(run, 0x8007fd98u, 0x80027b28u, 0));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0x80027b34u);
    } else {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = word(2);
        branch = equal_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_V1],
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]);
        if (branch < 0) {
            stop(run, 0x8007fd80u, 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (branch) {
            TRY(load_resource(run, 0x8007fdb8u, 0x80027b44u, 0));
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
                word(0x80027b50u);
        } else {
            TRY(load_resource(run, 0x8007fdd8u, 0x80027b60u, 0));
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
                word(0x80027b6cu);
        }
    }
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] = word(0x80100000u);
    TRY(write_word(run, AUX_EVENT_POINTER, 0x8007fdecu,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(0);
    TRY(invoke(run, 0x8007fdf0u, 0x80029bfcu,
        NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC, 2));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] = word(0x80100000u);
    TRY(write_word(run, AUX_NONEVENT_POINTER, 0x8007fdfcu,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));

    /* GAMEONLY 0x8007FE00..0x8007FE60: reread language after the loaders.
     * V0=2 executes in the first branch delay even when language is one. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] = word(0x80010000u);
    TRY(read_word(run, LANGUAGE_ADDRESS, 0x8007fe04u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]));
    out->second_language = run->registers.gpr[NBA97_MATCH_INITIALIZE_V1];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = word(1);
    condition = word((uint32_t)(equal_state(
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V1],
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]) == 1));
    if (equal_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_V1],
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]) < 0)
        condition.known_mask = 0x0e;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = word(2);
    branch = decide_branch(run, &condition, 0x8007fe0cu);
    if (branch < 0)
        return NBA97_TEXT_UNKNOWN;
    if (branch) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0x80027b78u);
    } else {
        condition = word((uint32_t)(equal_state(
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V1],
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]) == 1));
        if (equal_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_V1],
                &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]) < 0)
            condition.known_mask = 0x0e;
        branch = decide_branch(run, &condition, 0x8007fe14u);
        if (branch < 0)
            return NBA97_TEXT_UNKNOWN;
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(
            branch ? 0x80027b88u : 0x80027b98u);
    }
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(0x20u);
    TRY(invoke(run, 0x8007fe4cu, 0x80029bfcu,
        NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC, 2));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0x8007fb24u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3] =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    out->index_payload = run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3];
    TRY(invoke(run, 0x8007fe5cu, 0x800adbf8u,
        NBA97_GAME_SPEECH_INITIALIZE_INSTALL_800ADBF8, 1));

    /* GAMEONLY 0x8007FE64..0x8007FEAC: establish live cursors while opening
     * both auxiliary payloads, then reset the speech lookup service. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0x80100000u);
    TRY(read_word(run, AUX_EVENT_POINTER, 0x8007fe68u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_A0]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = word(0);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = word(0x80103220u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_FP] = word(0x80103190u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = word(HOME_TEAM_ADDRESS);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6] = word(AWAY_ROSTER_TABLE);
    TRY(invoke(run, 0x8007fe84u, 0x800aec00u,
        NBA97_GAME_SPEECH_INITIALIZE_OPEN_800AEC00, 1));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0x80100000u);
    TRY(read_word(run, AUX_NONEVENT_POINTER, 0x8007fe90u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_A0]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 5] = add_immediate(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 0x1d98u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 7] = word(RECORD_BASE);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4] = word(0);
    TRY(invoke(run, 0x8007fea0u, 0x800aec00u,
        NBA97_GAME_SPEECH_INITIALIZE_OPEN_800AEC00, 1));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0);
    TRY(invoke(run, 0x8007fea8u, 0x8007fa50u,
        NBA97_GAME_SPEECH_INITIALIZE_RESET_8007FA50, 1));

    /* GAMEONLY 0x8007FEB0..0x8007FF04: four team-level records. */
    TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0, 0x8007feb0u));
    TRY(read_half(run, run->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word,
        0x8007feb0u, 0, &value));
    TRY(lookup(run, 0x8007febcu, 4, value,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1]));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A2] = word(0x80020000u);
    TRY(read_half(run, AWAY_TEAM_ADDRESS, 0x8007fec8u, 0,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_A2]));
    TRY(lookup(run, 0x8007fed4u, 4,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A2],
        add_immediate(run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1], 12)));
    TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0, 0x8007fedcu));
    TRY(read_half(run, run->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word,
        0x8007fedcu, 0, &value));
    TRY(lookup(run, 0x8007fee8u, 5, value,
        add_immediate(run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1], 24)));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A2] = word(0x80020000u);
    TRY(read_half(run, AWAY_TEAM_ADDRESS, 0x8007fef4u, 0,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_A2]));
    TRY(lookup(run, 0x8007ff00u, 5,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A2],
        add_immediate(run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1], 36)));

    /* GAMEONLY 0x8007FF08..0x8007FFA4: twelve child-mutable home/away
     * roster iterations, with signed LH IDs and signed LB categories. */
    for (;;) {
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 5,
            0x8007ff08u));
        TRY(read_word(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 5].word,
            0x8007ff08u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_V0, 0x8007ff10u));
        TRY(read_half(run, run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word,
            0x8007ff10u, 1, &value));
        TRY(lookup(run, 0x8007ff1cu, 1, value,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 7]));

        TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 6,
            0x8007ff24u));
        TRY(read_word(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6].word,
            0x8007ff24u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 7] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 7], 12);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3];
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(1);
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_V0, 0x8007ff34u));
        TRY(read_half(run, run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word,
            0x8007ff34u, 1, &value));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_T0] = word(RECORD_BASE);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = word(0x80103070u);
        TRY(lookup(run, 0x8007ff44u, 1, value,
            add_words(run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4],
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0])));

        TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 5,
            0x8007ff4cu));
        TRY(read_word(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 5].word,
            0x8007ff4cu, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2], 1);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3];
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(3);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_T0] = word(RECORD_BASE);
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_V0, 0x8007ff64u));
        TRY(read_byte_signed(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word + 7u,
            0x8007ff64u, &value));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A3] = add_words(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4],
            word(0x80103100u));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 5] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 5], 4);
        TRY(lookup(run, 0x8007ff70u, 3, value,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A3]));

        TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 6,
            0x8007ff78u));
        TRY(read_word(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6].word,
            0x8007ff78u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4], 12);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3];
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(3);
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_V0, 0x8007ff88u));
        TRY(read_byte_signed(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word + 7u,
            0x8007ff88u, &value));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A3] =
            run->registers.gpr[NBA97_MATCH_INITIALIZE_FP];
        run->registers.gpr[NBA97_MATCH_INITIALIZE_FP] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_FP], 12);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6], 4);
        TRY(lookup(run, 0x8007ff94u, 3, value,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A3]));

        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            signed_less_immediate(
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2], 12);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 0x1e0u);
        branch = decide_branch(run,
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x8007ffa0u);
        if (branch < 0)
            return NBA97_TEXT_UNKNOWN;
        if (!branch)
            break;
    }

    /* GAMEONLY 0x8007FFA8..0x8007FFCC: 48 category-two records. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = word(0);
    for (;;) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_A2] =
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2];
        TRY(lookup(run, 0x8007ffb8u, 2,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A2],
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2], 1);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = signed_less_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2], 48);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 12);
        branch = decide_branch(run,
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x8007ffc8u);
        if (branch < 0)
            return NBA97_TEXT_UNKNOWN;
        if (!branch)
            break;
    }

    /* GAMEONLY 0x8007FFD0..0x8007FFF4: records 100..109 receive the source
     * -1 sentinel in field +4; arithmetic and addresses wrap naturally. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = word(100);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(UINT32_MAX);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] = word(0x4b0u);
    for (;;) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] = add_words(
            word(0x80100000u), run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]);
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_AT, 0x8007ffe4u));
        TRY(write_word(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_AT].word + 0x2fe4u,
            0x8007ffe4u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_A0]));
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2], 1);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = signed_less_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2], 110);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_V1], 12);
        branch = decide_branch(run,
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x8007fff0u);
        if (branch < 0)
            return NBA97_TEXT_UNKNOWN;
        if (!branch)
            break;
    }

    /* GAMEONLY 0x8007FFF8..0x80080038: sum +4 only for non-null records.
     * A partial pointer may still prove non-null from one known nonzero byte. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = word(RECORD_BASE);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] =
        add_immediate(run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 0x4b0u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = unsigned_less(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = word(0);
    branch = decide_branch(run,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x80080008u);
    if (branch < 0)
        return NBA97_TEXT_UNKNOWN;
    while (branch) {
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0, 0x80080010u));
        TRY(read_word(run, run->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word,
            0x80080010u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
        branch = zero_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]);
        if (branch < 0) {
            stop(run, 0x80080018u, 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (!branch) {
            TRY(read_word(run,
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word + 4u,
                0x80080020u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = add_words(
                run->registers.gpr[NBA97_MATCH_INITIALIZE_A1],
                run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]);
        }
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 12);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = unsigned_less(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
            run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]);
        branch = decide_branch(run,
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x80080034u);
        if (branch < 0)
            return NBA97_TEXT_UNKNOWN;
    }
    out->allocation_size = run->registers.gpr[NBA97_MATCH_INITIALIZE_A1];

    /* GAMEONLY 0x8008003C..0x80080048: unchecked allocation with flags zero. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = word(0x80027ba4u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A2] = word(0);
    TRY(invoke(run, 0x80080044u, 0x80090160u,
        NBA97_GAME_SPEECH_INITIALIZE_ALLOCATE_80090160, 3));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_AT] = word(0x80100000u);
    TRY(write_word(run, ALLOCATION_POINTER, 0x80080050u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
    out->allocated_buffer = run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0];

    /* GAMEONLY 0x80080058..0x800800B8: pack each non-null record. Callback
     * mutations of s0/s1/s2/s4 remain live for the loop and next addresses. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = word(RECORD_BASE);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V1] =
        add_immediate(run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 0x4b0u);
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = unsigned_less(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V1]);
    branch = decide_branch(run,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x80080068u);
    if (branch < 0)
        return NBA97_TEXT_UNKNOWN;
    if (branch) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4] =
            run->registers.gpr[NBA97_MATCH_INITIALIZE_V1];
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 4);
    }
    while (branch) {
        TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0, 0x80080078u));
        TRY(read_word(run, run->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word,
            0x80080078u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_A1]));
        branch = zero_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_A1]);
        if (branch < 0) {
            stop(run, 0x80080080u, 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (!branch) {
            TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 1,
                0x80080088u));
            TRY(read_word(run,
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word,
                0x80080088u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_A2]));
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2];
            TRY(invoke(run, 0x8008008cu, 0x8009cb0cu,
                NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C, 3));
            TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0,
                0x80080094u));
            TRY(write_word(run,
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word,
                0x80080094u,
                &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2]));
            TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 1,
                0x80080098u));
            TRY(read_word(run,
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word,
                0x80080098u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
            run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2];
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = add_words(
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2],
                run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]);
            TRY(invoke(run, 0x800800a0u, 0x800ae54cu,
                NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C, 1));
            TRY(require_register(run, NBA97_MATCH_INITIALIZE_S0 + 1,
                0x800800a8u));
            TRY(write_word(run,
                run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word,
                0x800800a8u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]));
        }
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0], 12);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = unsigned_less(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 4]);
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = add_immediate(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1], 12);
        branch = decide_branch(run,
            &run->registers.gpr[NBA97_MATCH_INITIALIZE_V0], 0x800800b4u);
        if (branch < 0)
            return NBA97_TEXT_UNKNOWN;
    }

    /* GAMEONLY 0x800800BC..0x800800F4: release the live index payload, then
     * reload the ten saved words through callback-mutable sp and validate ra
     * only when JR consumes it. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3];
    TRY(invoke(run, 0x800800bcu, 0x80090698u,
        NBA97_GAME_SPEECH_INITIALIZE_RELEASE_80090698, 1));
    TRY(require_register(run, NBA97_MATCH_INITIALIZE_SP, 0x800800c4u));
    for (i = 0; i < 10; ++i) {
        static const unsigned regs[10] = {
            NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_FP,
            NBA97_MATCH_INITIALIZE_S0 + 7, NBA97_MATCH_INITIALIZE_S0 + 6,
            NBA97_MATCH_INITIALIZE_S0 + 5, NBA97_MATCH_INITIALIZE_S0 + 4,
            NBA97_MATCH_INITIALIZE_S0 + 3, NBA97_MATCH_INITIALIZE_S0 + 2,
            NBA97_MATCH_INITIALIZE_S0 + 1, NBA97_MATCH_INITIALIZE_S0
        };
        uint32_t offset = 0x34u - i * 4u;
        TRY(read_word(run,
            run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + offset,
            0x800800c4u + i * 4u, &run->registers.gpr[regs[i]]));
        out->restored_register[i] = run->registers.gpr[regs[i]];
    }
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_immediate(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP], 0x38u);
    if (run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 0x0fu) {
        stop(run, 0x800800f0u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
