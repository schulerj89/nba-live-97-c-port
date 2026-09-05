#include "game_scene_random_warmup.h"

#include <string.h>

typedef struct Nba97GameSceneRandomWarmupRun {
    Nba97GameSceneRandomWarmupContext* context;
    Nba97GameSceneRandomWarmupProgress* out;
    Nba97GameSceneRandomWarmupRegisters registers;
} Nba97GameSceneRandomWarmupRun;

#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameSceneRandomWarmupRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameSceneRandomWarmupRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int spend(Nba97GameSceneRandomWarmupRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameSceneRandomWarmupRun* run, uint8_t kind,
    uint32_t pc, uint32_t address,
    const Nba97GameSceneRandomWarmupWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameSceneRandomWarmupAccess* event =
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

static int locate(Nba97GameSceneRandomWarmupRun* run, uint32_t address,
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

static int read_word(Nba97GameSceneRandomWarmupRun* run, uint32_t address,
    uint32_t pc, Nba97GameSceneRandomWarmupWord* value) {
    Nba97GameSceneRandomWarmupWord loaded = {0, 0};
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    for (i = 0; i < 4; ++i) {
        loaded.word |= (uint32_t)data[i] << (i * 8u);
        if (!known || known[i])
            loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_SCENE_RANDOM_WARMUP_READ,
        pc, address, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameSceneRandomWarmupRun* run, uint32_t address,
    uint32_t pc, const Nba97GameSceneRandomWarmupWord* value) {
    uint8_t* data;
    uint8_t* known;
    unsigned i;
    TRY(locate(run, address, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_SCENE_RANDOM_WARMUP_STORE,
        pc, address, value);
    return NBA97_TEXT_COMPLETE;
}

static int registers_valid(
    const Nba97GameSceneRandomWarmupRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameSceneRandomWarmupContext* context,
    Nba97GameSceneRandomWarmupProgress* out,
    Nba97GameSceneRandomWarmupRun* run) {
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

static int require_register(Nba97GameSceneRandomWarmupRun* run,
    unsigned index, uint32_t pc) {
    if (run->registers.gpr[index].known_mask != 0x0fu) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static size_t invocation(const Nba97GameSceneRandomWarmupProgress* out,
    uint8_t kind) {
    switch (kind) {
    case NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8:
        return out->startup_calls + 1u;
    case NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70:
        return out->random_calls + 1u;
    case NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694:
        return out->seed_calls + 1u;
    default:
        return out->step_calls + 1u;
    }
}

static void count_call(Nba97GameSceneRandomWarmupProgress* out,
    uint8_t kind) {
    switch (kind) {
    case NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8:
        ++out->startup_calls;
        break;
    case NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70:
        ++out->random_calls;
        break;
    case NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694:
        ++out->seed_calls;
        break;
    default:
        ++out->step_calls;
        break;
    }
}

static int invoke(Nba97GameSceneRandomWarmupRun* run, uint32_t pc,
    uint32_t entry, uint32_t delay_slot_pc, uint8_t kind,
    uint8_t argument_count) {
    Nba97GameSceneRandomWarmupEvent event;
    int accepted;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = pc + 8u;
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x0fu;
    stop(run, pc, 0, entry);
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = delay_slot_pc;
    event.entry = entry;
    event.operation = run->out->operations;
    event.invocation = invocation(run->out, kind);
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
    count_call(run->out, kind);
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameSceneRandomWarmupWord and_immediate(
    Nba97GameSceneRandomWarmupWord value, uint32_t immediate) {
    unsigned i;
    value.word &= immediate;
    for (i = 0; i < 4; ++i)
        if (((immediate >> (i * 8u)) & 0xffu) == 0)
            value.known_mask =
                (uint8_t)(value.known_mask | (uint8_t)(1u << i));
    return value;
}

static Nba97GameSceneRandomWarmupWord add_immediate(
    Nba97GameSceneRandomWarmupWord value, uint32_t immediate) {
    uint32_t original = value.word;
    uint8_t original_known = value.known_mask;
    uint8_t result_known = 0;
    unsigned carry = 0;
    unsigned carry_known = 1;
    unsigned i;
    value.word += immediate;
    for (i = 0; i < 4; ++i) {
        unsigned byte_known = (original_known >> i) & 1u;
        unsigned byte = (original >> (i * 8u)) & 0xffu;
        unsigned addend = (immediate >> (i * 8u)) & 0xffu;
        if (byte_known && carry_known)
            result_known = (uint8_t)(result_known | (uint8_t)(1u << i));
        if (byte_known && carry_known) {
            carry = byte + addend + carry > 0xffu;
        } else if (!byte_known && carry_known && addend + carry == 0) {
            carry = 0;
            carry_known = 1;
        } else if (!byte_known && carry_known && addend + carry == 0x100u) {
            carry = 1;
            carry_known = 1;
        } else if (byte_known && !carry_known && byte + addend != 0xffu) {
            carry = byte + addend > 0xffu;
            carry_known = 1;
        } else {
            carry_known = 0;
        }
    }
    value.known_mask = result_known;
    return value;
}

static Nba97GameSceneRandomWarmupWord warmup_count(
    Nba97GameSceneRandomWarmupWord random_value) {
    Nba97GameSceneRandomWarmupWord value =
        and_immediate(random_value, UINT32_C(0x0000007f));
    value.word += 0x40u;
    /* The masked source range is 0..127, so adding 64 cannot carry out of
     * the low byte. The three already-known zero bytes remain known. */
    return value;
}

static Nba97GameSceneRandomWarmupWord decrement(
    Nba97GameSceneRandomWarmupWord value) {
    uint32_t original = value.word;
    uint8_t original_known = value.known_mask;
    uint8_t result_known = 0;
    unsigned borrow = 1;
    unsigned borrow_known = 1;
    unsigned i;
    value.word -= 1u;
    for (i = 0; i < 4; ++i) {
        unsigned byte_known = (original_known >> i) & 1u;
        unsigned byte = (original >> (i * 8u)) & 0xffu;
        if (byte_known && borrow_known)
            result_known = (uint8_t)(result_known | (uint8_t)(1u << i));
        if (!borrow_known) {
            if (byte_known && byte != 0) {
                borrow = 0;
                borrow_known = 1;
            }
        } else if (!borrow) {
            borrow_known = 1;
        } else if (!byte_known) {
            borrow_known = 0;
        } else {
            borrow = byte == 0;
        }
    }
    value.known_mask = result_known;
    return value;
}

/* Return 1 for known zero, 0 for known nonzero, and -1 when unknown bytes can
 * change the branch outcome. */
static int zero_state(const Nba97GameSceneRandomWarmupWord* value) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (i * 8u)) & 0xffu) != 0)
            return 0;
    return value->known_mask == 0x0fu ? 1 : -1;
}

int nba97_game_scene_random_warmup(
    Nba97GameSceneRandomWarmupContext* context,
    Nba97GameSceneRandomWarmupProgress* out) {
    Nba97GameSceneRandomWarmupRun storage;
    Nba97GameSceneRandomWarmupRun* run = &storage;
    int zero;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800802AC..0x800802B0: ADDIU always forms the raw frame sp
     * and propagates byte/carry knownness. SW is its first concrete use. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = add_immediate(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
        UINT32_C(0xffffffe8));
    out->frame_stack_pointer =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
    TRY(require_register(run, NBA97_MATCH_INITIALIZE_SP, 0x800802b0u));
    TRY(write_word(run, out->frame_stack_pointer + 0x14u, 0x800802b0u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));

    /* The 0x800802B8 delay slot saves entry s0 after JAL has assigned ra. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        (Nba97GameSceneRandomWarmupWord){UINT32_C(0x800802bc), 0x0f};
    TRY(write_word(run, out->frame_stack_pointer + 0x10u, 0x800802b8u,
        &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    TRY(invoke(run, 0x800802b4u, 0x800800f8u, 0x800802b8u,
        NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8, 0));

    /* GAMEONLY 0x800802BC..0x800802CC: retain only low 7 bits of the first
     * random return and assign count+64 to s0 in the second JAL delay slot. */
    TRY(invoke(run, 0x800802bcu, 0x8002ab70u, 0x800802c0u,
        NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70, 0));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = and_immediate(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
        UINT32_C(0x0000007f));
    run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = warmup_count(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0]);
    out->warmup_count =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0];
    TRY(invoke(run, 0x800802c8u, 0x8002ab70u, 0x800802ccu,
        NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70, 0));

    /* The second random result is truncated in the seed child's JAL delay. */
    run->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = and_immediate(
        run->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
        UINT32_C(0x0000ffff));
    out->seed_argument = run->registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    TRY(invoke(run, 0x800802d0u, 0x80093694u, 0x800802d4u,
        NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694, 1));

    /* GAMEONLY 0x800802D8..0x800802EC: the seed child may replace s0. Each
     * step receives s0 after the source decrement in its JAL delay slot, and
     * the following BNE tests the child's live replacement. */
    zero = zero_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]);
    if (zero < 0) {
        stop(run, 0x800802d8u, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    while (!zero) {
        run->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = decrement(
            run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]);
        TRY(invoke(run, 0x800802e0u, 0x800935c4u, 0x800802e4u,
            NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4, 0));
        zero = zero_state(&run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]);
        if (zero < 0) {
            stop(run, 0x800802e8u, 0, 0);
            return NBA97_TEXT_UNKNOWN;
        }
    }

    /* GAMEONLY 0x800802F0..0x80080300: both loads use the child-mutable live
     * sp. Restore that sp before JR validates the reloaded ra. */
    TRY(require_register(run, NBA97_MATCH_INITIALIZE_SP, 0x800802f0u));
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x14u,
        0x800802f0u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    out->restored_return_address =
        run->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    TRY(read_word(run,
        run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x10u,
        0x800802f4u, &run->registers.gpr[NBA97_MATCH_INITIALIZE_S0]));
    out->restored_s0 = run->registers.gpr[NBA97_MATCH_INITIALIZE_S0];
    run->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 0x18u;
    if (out->restored_return_address.known_mask != 0x0fu) {
        stop(run, 0x800802fcu, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
