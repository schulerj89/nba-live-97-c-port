#include "game_camera_startup.h"

#include <string.h>

typedef struct Nba97GameCameraStartupRun {
    Nba97GameCameraStartupContext* context;
    Nba97GameCameraStartupProgress* out;
    Nba97GameCameraStartupRegisters registers;
} Nba97GameCameraStartupRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_status_ = (expression); \
    if (nba97_status_ != NBA97_TEXT_COMPLETE) return nba97_status_; \
} while (0)

static void publish(Nba97GameCameraStartupRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameCameraStartupRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameCameraStartupWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static Nba97GameCameraStartupWord add_words(
    Nba97GameCameraStartupWord left, Nba97GameCameraStartupWord right) {
    Nba97GameCameraStartupWord result;
    unsigned i;
    unsigned carry_min = 0;
    unsigned carry_max = 0;
    result.word = left.word + right.word;
    result.known_mask = 0;
    for (i = 0; i < 4; ++i) {
        const unsigned left_byte = (left.word >> (8u * i)) & 0xffu;
        const unsigned right_byte = (right.word >> (8u * i)) & 0xffu;
        const unsigned left_min = (left.known_mask & (1u << i)) ?
            left_byte : 0u;
        const unsigned left_max = (left.known_mask & (1u << i)) ?
            left_byte : 255u;
        const unsigned right_min = (right.known_mask & (1u << i)) ?
            right_byte : 0u;
        const unsigned right_max = (right.known_mask & (1u << i)) ?
            right_byte : 255u;
        const unsigned sum_min = left_min + right_min + carry_min;
        const unsigned sum_max = left_max + right_max + carry_max;
        if (sum_min == sum_max)
            result.known_mask = (uint8_t)(result.known_mask | (1u << i));
        carry_min = sum_min >> 8u;
        carry_max = sum_max >> 8u;
    }
    return result;
}

static int equality_known(const Nba97GameCameraStartupWord* left,
    const Nba97GameCameraStartupWord* right, int* equal) {
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
    if (left->known_mask == 0x0fu && right->known_mask == 0x0fu) {
        *equal = left->word == right->word;
        return 1;
    }
    return 0;
}

static int spend(Nba97GameCameraStartupRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int require_known(Nba97GameCameraStartupRun* run,
    const Nba97GameCameraStartupWord* value, uint32_t pc,
    uint32_t address, uint32_t entry) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, address, entry);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameCameraStartupRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width, uint32_t word,
    uint8_t known_mask) {
    const size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameCameraStartupAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = word;
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask = known_mask;
        event->kind = kind;
    }
}

static int locate(Nba97GameCameraStartupRun* run, uint32_t address,
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
        const uint64_t offset = (uint64_t)address - region->base;
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

static int read_value(Nba97GameCameraStartupRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    Nba97GameCameraStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameCameraStartupWord result;
    unsigned i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    result.word = 0;
    result.known_mask = width == 1 ? 0x0eu : 0;
    for (i = 0; i < width; ++i) {
        result.word |= (uint32_t)data[i] << (8u * i);
        if (!known || known[i])
            result.known_mask = (uint8_t)(
                result.known_mask | (1u << i));
    }
    *value = result;
    ++run->out->reads;
    journal(run, NBA97_GAME_CAMERA_STARTUP_READ, pc, address,
        (uint8_t)width, result.word,
        (uint8_t)(result.known_mask & ((1u << width) - 1u)));
    return NBA97_TEXT_COMPLETE;
}

static int read_byte(Nba97GameCameraStartupRun* run, uint32_t address,
    uint32_t pc, Nba97GameCameraStartupWord* value) {
    return read_value(run, address, 1, 1, pc, value);
}

static int read_word(Nba97GameCameraStartupRun* run, uint32_t address,
    uint32_t pc, Nba97GameCameraStartupWord* value) {
    return read_value(run, address, 4, 4, pc, value);
}

static int write_value(Nba97GameCameraStartupRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    const Nba97GameCameraStartupWord* value) {
    uint8_t* data;
    uint8_t* known;
    const uint8_t width_mask = (uint8_t)((1u << width) - 1u);
    unsigned i;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    if (!known && (value->known_mask & width_mask) != width_mask)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < width; ++i) {
        data[i] = (uint8_t)(value->word >> (8u * i));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_CAMERA_STARTUP_STORE, pc, address,
        (uint8_t)width, value->word,
        (uint8_t)(value->known_mask & width_mask));
    return NBA97_TEXT_COMPLETE;
}

static int write_byte(Nba97GameCameraStartupRun* run, uint32_t address,
    uint32_t pc, const Nba97GameCameraStartupWord* value) {
    return write_value(run, address, 1, 1, pc, value);
}

static int write_word(Nba97GameCameraStartupRun* run, uint32_t address,
    uint32_t pc, const Nba97GameCameraStartupWord* value) {
    return write_value(run, address, 4, 4, pc, value);
}

static int write_known_word(Nba97GameCameraStartupRun* run,
    uint32_t address, uint32_t pc, uint32_t word) {
    Nba97GameCameraStartupWord value;
    set_known(&value, word);
    return write_word(run, address, pc, &value);
}

static int registers_valid(const Nba97GameCameraStartupRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameCameraStartupContext* context,
    Nba97GameCameraStartupProgress* out,
    Nba97GameCameraStartupRun* run) {
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

static int add_immediate(Nba97GameCameraStartupWord* value,
    uint32_t immediate) {
    Nba97GameCameraStartupWord known;
    set_known(&known, immediate);
    *value = add_words(*value, known);
    return NBA97_TEXT_COMPLETE;
}

static int stack_write(Nba97GameCameraStartupRun* run, uint32_t offset,
    uint32_t pc, const Nba97GameCameraStartupWord* value) {
    Nba97GameCameraStartupWord address = R(NBA97_MATCH_INITIALIZE_SP);
    TRY(add_immediate(&address, offset));
    TRY(require_known(run, &address, pc, address.word, 0));
    return write_word(run, address.word, pc, value);
}

static int stack_read(Nba97GameCameraStartupRun* run, uint32_t offset,
    uint32_t pc, Nba97GameCameraStartupWord* value) {
    Nba97GameCameraStartupWord address = R(NBA97_MATCH_INITIALIZE_SP);
    TRY(add_immediate(&address, offset));
    TRY(require_known(run, &address, pc, address.word, 0));
    return read_word(run, address.word, pc, value);
}

static int invoke(Nba97GameCameraStartupRun* run, uint32_t pc) {
    Nba97GameCameraStartupEvent event;
    int accepted;
    stop(run, pc, 0, UINT32_C(0x800799cc));
    TRY(spend(run));
    memset(&event, 0, sizeof event);
    event.pc = pc;
    event.delay_slot_pc = pc + 4u;
    event.entry = UINT32_C(0x800799cc);
    event.operation = run->out->operations;
    event.kind = NBA97_GAME_CAMERA_STARTUP_CHILD_800799CC;
    event.argument_count = 2;
    publish(run);
    if (!run->context->io)
        return NBA97_TEXT_IO_REFUSED;
    accepted = run->context->io(run->context->user,
        &run->context->memory, &event, &run->registers);
    publish(run);
    if (accepted != 1)
        return NBA97_TEXT_IO_REFUSED;
    if (!registers_valid(&run->registers))
        return NBA97_TEXT_ARGUMENT;
    ++run->out->callbacks_completed;
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_camera_startup(Nba97GameCameraStartupContext* context,
    Nba97GameCameraStartupProgress* out) {
    Nba97GameCameraStartupRun storage;
    Nba97GameCameraStartupRun* run = &storage;
    Nba97GameCameraStartupWord one;
    int branch_known;
    int a0_is_one;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x80079664..0x80079670: the two unsigned-byte loads happen
     * before the stack frame and retain byte-level knownness. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
    TRY(read_byte(run, UINT32_C(0x80021ed9), UINT32_C(0x80079668),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    out->initial_camera_byte = R(NBA97_MATCH_INITIALIZE_V1);
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020000));
    TRY(read_byte(run, UINT32_C(0x80021eda), UINT32_C(0x80079670),
        &R(NBA97_MATCH_INITIALIZE_A1)));
    out->initial_aux_byte = R(NBA97_MATCH_INITIALIZE_A1);

    /* 0x80079674..0x800796AC: form the live frame, publish constants, save
     * ra, then publish the two bytes in exact source order. */
    TRY(add_immediate(&R(NBA97_MATCH_INITIALIZE_SP),
        UINT32_C(0xffffffe8)));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x80104744), UINT32_C(0x80079680),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x00000100));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    TRY(write_word(run, UINT32_C(0x800bc258), UINT32_C(0x8007968c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x800fc9b4), UINT32_C(0x80079694),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    TRY(stack_write(run, 0x10u, UINT32_C(0x8007969c),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_byte(run, UINT32_C(0x800fa378), UINT32_C(0x800796a4),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_byte(run, UINT32_C(0x800fabc4), UINT32_C(0x800796ac),
        &R(NBA97_MATCH_INITIALIZE_A1)));

    /* 0x800796B0..0x800796E8: BEQ samples entry a0 before its unconditional
     * a0=12 delay slot. Unknown control flow stops only after that slot. */
    set_known(&one, 1);
    branch_known = equality_known(&R(NBA97_MATCH_INITIALIZE_A0), &one,
        &a0_is_one);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 12);
    if (!branch_known) {
        stop(run, UINT32_C(0x800796b0), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    if (!a0_is_one) {
        set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800796c0));
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(invoke(run, UINT32_C(0x800796b8)));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
        TRY(write_byte(run, UINT32_C(0x801029bc),
            UINT32_C(0x800796c8), &R(NBA97_MATCH_INITIALIZE_V0)));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
        TRY(write_known_word(run, UINT32_C(0x800dce00),
            UINT32_C(0x800796d0), 0));
        /* 0x800796D4 J has a NOP delay at 0x800796D8. */
    } else {
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
        TRY(read_byte(run, UINT32_C(0x80021ed7),
            UINT32_C(0x800796e0), &R(NBA97_MATCH_INITIALIZE_A0)));
        set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800796ec));
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
        TRY(invoke(run, UINT32_C(0x800796e4)));
    }

    /* 0x800796EC..0x80079744: reload the three live source words, clear five
     * camera words, and publish the sources in exact load/store order. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800c0000));
    TRY(read_word(run, UINT32_C(0x800bc3d4), UINT32_C(0x800796f0),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800c0000));
    TRY(read_word(run, UINT32_C(0x800bc3d8), UINT32_C(0x800796f8),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x800c0000));
    TRY(read_word(run, UINT32_C(0x800bc3dc), UINT32_C(0x80079700),
        &R(NBA97_MATCH_INITIALIZE_A1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x801042ac),
        UINT32_C(0x8007970c), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x801042b0),
        UINT32_C(0x80079714), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x801042b4),
        UINT32_C(0x8007971c), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_known_word(run, UINT32_C(0x80106074),
        UINT32_C(0x80079724), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    TRY(write_word(run, UINT32_C(0x800bc1f4), UINT32_C(0x8007972c),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x8010607c), UINT32_C(0x80079734),
        &R(NBA97_MATCH_INITIALIZE_V1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x80106080), UINT32_C(0x8007973c),
        &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x80106084), UINT32_C(0x80079744),
        &R(NBA97_MATCH_INITIALIZE_A1)));

    /* 0x80079748..0x80079754: reload ra through live sp, advance that same
     * sp with byte-known carry propagation, then require known ra for JR. */
    TRY(stack_read(run, 0x10u, UINT32_C(0x80079748),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(add_immediate(&R(NBA97_MATCH_INITIALIZE_SP), 0x18u));
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80079750),
            R(NBA97_MATCH_INITIALIZE_RA).word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    /* JR at 0x80079750 executes the NOP delay slot at 0x80079754. */
    out->completed = 1;
    out->stopped_pc = 0;
    out->stopped_address = 0;
    out->stopped_entry = 0;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}
