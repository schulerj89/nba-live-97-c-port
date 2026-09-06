#include "game_speech_startup.h"

#include <limits.h>
#include <string.h>

typedef struct Nba97GameSpeechStartupRun {
    Nba97GameSpeechStartupContext* context;
    Nba97GameSpeechStartupProgress* out;
    Nba97GameSpeechStartupRegisters registers;
} Nba97GameSpeechStartupRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameSpeechStartupRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameSpeechStartupRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int registers_valid(const Nba97GameSpeechStartupRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameSpeechStartupContext* context,
    Nba97GameSpeechStartupProgress* out, Nba97GameSpeechStartupRun* run) {
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

static void set_known(Nba97GameSpeechStartupWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static Nba97GameSpeechStartupWord add_constant(
    Nba97GameSpeechStartupWord source, uint32_t addend) {
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
            carry_known = 1;
        } else if (!byte_known && carry_known && add_byte + carry == 0x100u) {
            carry = 1;
            carry_known = 1;
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

static int spend(Nba97GameSpeechStartupRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameSpeechStartupRun* run, uint8_t kind,
    uint32_t pc, uint32_t address,
    const Nba97GameSpeechStartupWord* value) {
    const size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameSpeechStartupAccess* event =
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

static int locate(Nba97GameSpeechStartupRun* run, uint32_t address,
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

static int read_word(Nba97GameSpeechStartupRun* run, uint32_t address,
    uint32_t pc, Nba97GameSpeechStartupWord* value) {
    Nba97GameSpeechStartupWord loaded = {0, 0};
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
    journal(run, NBA97_GAME_SPEECH_STARTUP_READ, pc, address, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameSpeechStartupRun* run, uint32_t address,
    uint32_t pc, const Nba97GameSpeechStartupWord* value) {
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
    journal(run, NBA97_GAME_SPEECH_STARTUP_STORE, pc, address, value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int require_known(Nba97GameSpeechStartupRun* run,
    const Nba97GameSpeechStartupWord* value, uint32_t pc) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, value->word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int stack_write(Nba97GameSpeechStartupRun* run, uint32_t offset,
    uint32_t pc, const Nba97GameSpeechStartupWord* value) {
    Nba97GameSpeechStartupWord address = R(NBA97_MATCH_INITIALIZE_SP);
    address = add_constant(address, offset);
    TRY(require_known(run, &address, pc));
    return write_word(run, address.word, pc, value);
}

static int stack_read(Nba97GameSpeechStartupRun* run, uint32_t offset,
    uint32_t pc, Nba97GameSpeechStartupWord* value) {
    Nba97GameSpeechStartupWord address = R(NBA97_MATCH_INITIALIZE_SP);
    address = add_constant(address, offset);
    TRY(require_known(run, &address, pc));
    return read_word(run, address.word, pc, value);
}

static int invoke(Nba97GameSpeechStartupRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count) {
    Nba97GameSpeechStartupEvent event;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = run->out->call_count[kind] + 1u;
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

/* Return 1 for known zero, 0 for known nonzero, and -1 when an unknown byte
 * can change the branch result. */
static int zero_state(const Nba97GameSpeechStartupWord* value) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (8u * i)) & 0xffu) != 0)
            return 0;
    return value->known_mask == 0x0fu ? 1 : -1;
}

static int equality_constant(const Nba97GameSpeechStartupWord* value,
    uint32_t constant, int* equal) {
    unsigned i;
    for (i = 0; i < 4; ++i) {
        const uint8_t bit = (uint8_t)(1u << i);
        if ((value->known_mask & bit) &&
            ((value->word >> (8u * i)) & 0xffu) !=
            ((constant >> (8u * i)) & 0xffu)) {
            *equal = 0;
            return 1;
        }
    }
    if (value->known_mask == 0x0fu) {
        *equal = value->word == constant;
        return 1;
    }
    return 0;
}

static void signed_bounds(const Nba97GameSpeechStartupWord* value,
    int64_t* minimum, int64_t* maximum) {
    unsigned i;
    uint32_t low = 0;
    uint32_t high = 0;
    if (!(value->known_mask & 8u)) {
        *minimum = INT32_MIN;
        *maximum = INT32_MAX;
        return;
    }
    for (i = 0; i < 4; ++i) {
        const uint32_t byte = (value->word >> (8u * i)) & 0xffu;
        low |= ((value->known_mask & (1u << i)) ? byte : 0u) << (8u * i);
        high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) << (8u * i);
    }
    *minimum = low < UINT32_C(0x80000000) ? (int64_t)low :
        (int64_t)low - INT64_C(0x100000000);
    *maximum = high < UINT32_C(0x80000000) ? (int64_t)high :
        (int64_t)high - INT64_C(0x100000000);
}

static int64_t signed_word(uint32_t word) {
    return word < UINT32_C(0x80000000) ? (int64_t)word :
        (int64_t)word - INT64_C(0x100000000);
}

static int signed_less_than(const Nba97GameSpeechStartupWord* left,
    const Nba97GameSpeechStartupWord* right,
    Nba97GameSpeechStartupWord* result) {
    int64_t left_min;
    int64_t left_max;
    int64_t right_min;
    int64_t right_max;
    signed_bounds(left, &left_min, &left_max);
    signed_bounds(right, &right_min, &right_max);
    result->word = signed_word(left->word) < signed_word(right->word);
    /* SLT can leave only its low byte uncertain; the upper 24 result bits
     * are zero for either possible source outcome. */
    result->known_mask = 0x0eu;
    if (left_max < right_min) {
        set_known(result, 1);
        return 1;
    }
    if (left_min >= right_max) {
        set_known(result, 0);
        return 1;
    }
    return 0;
}

int nba97_game_speech_startup(Nba97GameSpeechStartupContext* context,
    Nba97GameSpeechStartupProgress* out) {
    Nba97GameSpeechStartupRun storage;
    Nba97GameSpeechStartupRun* run = &storage;
    Nba97GameSpeechStartupWord zero;
    int branch_known;
    int branch_taken;
    TRY(validate(context, out, run));
    set_known(&zero, 0);

    /* GAMEONLY 0x800800F8..0x80080110: form the 0x20-byte frame, save
     * entry ra/s0, then clear both retained speech globals in source order. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(stack_write(run, 0x1cu, UINT32_C(0x800800fc),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(stack_write(run, 0x18u, UINT32_C(0x80080100),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x80103fb0), UINT32_C(0x80080108), &zero));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    TRY(write_word(run, UINT32_C(0x800c4568), UINT32_C(0x80080110), &zero));
    TRY(invoke(run, UINT32_C(0x80080114), UINT32_C(0x8002a1b8),
        NBA97_GAME_SPEECH_STARTUP_CHILD_8002A1B8, 0));

    /* 0x8008011C..0x80080140: allocate the speech service, read language
     * before moving v0 to a0, and publish the raw possibly-partial handle. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x6000));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x2000));
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x20));
    TRY(invoke(run, UINT32_C(0x80080124), UINT32_C(0x800853f4),
        NBA97_GAME_SPEECH_STARTUP_CHILD_800853F4, 3));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80010000));
    TRY(read_word(run, UINT32_C(0x80015018), UINT32_C(0x80080130),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    out->language = R(NBA97_MATCH_INITIALIZE_V1);
    R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
    TRY(write_word(run, UINT32_C(0x8002149c), UINT32_C(0x80080140),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    out->speech_handle = R(NBA97_MATCH_INITIALIZE_A0);

    /* 0x80080144..0x80080188: both BEQs preserve their delay slots. An
     * indeterminate language stops after v0=2 from the first delay slot. */
    branch_known = equality_constant(&R(NBA97_MATCH_INITIALIZE_V1), 1,
        &branch_taken);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
    if (!branch_known) {
        stop(run, UINT32_C(0x80080144), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    if (branch_taken) {
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80027bb0));
    } else {
        branch_known = equality_constant(&R(NBA97_MATCH_INITIALIZE_V1), 2,
            &branch_taken);
        if (!branch_known) {
            stop(run, UINT32_C(0x8008014c), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (branch_taken) {
            set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80027bc0));
        } else {
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
            TRY(read_word(run, UINT32_C(0x8002149c),
                UINT32_C(0x80080180), &R(NBA97_MATCH_INITIALIZE_A0)));
            set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80027bd0));
        }
    }
    TRY(invoke(run, UINT32_C(0x8008018c), UINT32_C(0x800859c8),
        NBA97_GAME_SPEECH_STARTUP_CHILD_800859C8, 2));

    /* 0x80080194..0x800801C0: configure playback and write the fifth
     * argument through the live child-mutable sp in the JAL delay slot. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0x3cu);
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0x400u);
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0);
    TRY(invoke(run, UINT32_C(0x8008019c), UINT32_C(0x800889f4),
        NBA97_GAME_SPEECH_STARTUP_CHILD_800889F4, 3));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    TRY(read_word(run, UINT32_C(0x8002149c), UINT32_C(0x800801a8),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 5);
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x2710));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    set_known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x6000));
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800801c4));
    TRY(stack_write(run, 0x10u, UINT32_C(0x800801c0),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(invoke(run, UINT32_C(0x800801bc), UINT32_C(0x80029ca0),
        NBA97_GAME_SPEECH_STARTUP_CHILD_80029CA0, 5));

    /* 0x800801C4..0x800801E0: create and publish the raw voice value before
     * the following child sees it in a0; its JAL delay clears a1. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 15);
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0xffffffff));
    TRY(invoke(run, UINT32_C(0x800801c8), UINT32_C(0x80083d38),
        NBA97_GAME_SPEECH_STARTUP_CHILD_80083D38, 2));
    R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
    TRY(write_word(run, UINT32_C(0x800dc7e8), UINT32_C(0x800801d8),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    out->published_voice = R(NBA97_MATCH_INITIALIZE_A0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
    TRY(invoke(run, UINT32_C(0x800801dc), UINT32_C(0x800abfbc),
        NBA97_GAME_SPEECH_STARTUP_CHILD_800ABFBC, 2));

    /* 0x800801E4..0x800801F4: one initial pump precedes the clock sample;
     * the +240 deadline wraps and retains byte/carry knownness. */
    TRY(invoke(run, UINT32_C(0x800801e4), UINT32_C(0x80083eec),
        NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC, 0));
    TRY(invoke(run, UINT32_C(0x800801ec), UINT32_C(0x800a5810),
        NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810, 0));
    R(NBA97_MATCH_INITIALIZE_S0) = add_constant(
        R(NBA97_MATCH_INITIALIZE_V0), 0xf0u);
    out->deadline = R(NBA97_MATCH_INITIALIZE_S0);

    /* 0x800801F8..0x80080228: ready wins first. Otherwise compare deadline
     * and live clock as signed words; equality pumps and retries. Both BNE
     * delay slots clear a0 even when branch direction is unknowable. */
    for (;;) {
        TRY(invoke(run, UINT32_C(0x800801f8), UINT32_C(0x8008847c),
            NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C, 0));
        branch_taken = zero_state(&R(NBA97_MATCH_INITIALIZE_V0));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
        if (branch_taken < 0) {
            stop(run, UINT32_C(0x80080200), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (!branch_taken)
            break;
        TRY(invoke(run, UINT32_C(0x80080208), UINT32_C(0x800a5810),
            NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810, 0));
        branch_known = signed_less_than(&R(NBA97_MATCH_INITIALIZE_S0),
            &R(NBA97_MATCH_INITIALIZE_V0),
            &R(NBA97_MATCH_INITIALIZE_V0));
        branch_taken = R(NBA97_MATCH_INITIALIZE_V0).word != 0;
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
        if (!branch_known) {
            stop(run, UINT32_C(0x80080214), 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
        if (branch_taken)
            break;
        TRY(invoke(run, UINT32_C(0x8008021c), UINT32_C(0x80083eec),
            NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC, 0));
    }

    /* 0x8008022C..0x80080244: cleanup receives a0/a1 zero, then ra/s0 are
     * restored through cleanup's live sp before that same sp advances. */
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
    TRY(invoke(run, UINT32_C(0x8008022c), UINT32_C(0x8002abb4),
        NBA97_GAME_SPEECH_STARTUP_CHILD_8002ABB4, 2));
    TRY(stack_read(run, 0x1cu, UINT32_C(0x80080234),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(stack_read(run, 0x18u, UINT32_C(0x80080238),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    out->restored_s0 = R(NBA97_MATCH_INITIALIZE_S0);
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80080240),
            R(NBA97_MATCH_INITIALIZE_RA).word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
