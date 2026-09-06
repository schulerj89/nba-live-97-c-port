#include "game_camera_select.h"

#include <string.h>

typedef struct Nba97GameCameraSelectRun {
    Nba97GameCameraSelectContext* context;
    Nba97GameCameraSelectProgress* out;
    Nba97GameCameraSelectRegisters registers;
} Nba97GameCameraSelectRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameCameraSelectRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameCameraSelectRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static void set_known(Nba97GameCameraSelectWord* value, uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

static Nba97GameCameraSelectWord add_words(
    Nba97GameCameraSelectWord left, Nba97GameCameraSelectWord right) {
    Nba97GameCameraSelectWord result;
    unsigned byte;
    unsigned carry_min = 0;
    unsigned carry_max = 0;
    result.word = left.word + right.word;
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte) {
        unsigned bit = 1u << byte;
        unsigned left_byte = (left.word >> (byte * 8u)) & 0xffu;
        unsigned right_byte = (right.word >> (byte * 8u)) & 0xffu;
        unsigned left_min = (left.known_mask & bit) ? left_byte : 0;
        unsigned left_max = (left.known_mask & bit) ? left_byte : 0xffu;
        unsigned right_min = (right.known_mask & bit) ? right_byte : 0;
        unsigned right_max = (right.known_mask & bit) ? right_byte : 0xffu;
        unsigned sum_min = left_min + right_min + carry_min;
        unsigned sum_max = left_max + right_max + carry_max;
        if (sum_min == sum_max)
            result.known_mask = (uint8_t)(result.known_mask | bit);
        carry_min = sum_min >> 8u;
        carry_max = sum_max >> 8u;
    }
    return result;
}

static Nba97GameCameraSelectWord shift_left(
    Nba97GameCameraSelectWord source, unsigned amount) {
    Nba97GameCameraSelectWord result;
    uint32_t known_bits = 0;
    uint32_t shifted_known;
    unsigned byte;
    for (byte = 0; byte < 4; ++byte)
        if (source.known_mask & (1u << byte))
            known_bits |= UINT32_C(0xff) << (byte * 8u);
    result.word = source.word << amount;
    shifted_known = (known_bits << amount) |
        ((UINT32_C(1) << amount) - 1u);
    result.known_mask = 0;
    for (byte = 0; byte < 4; ++byte)
        if (((shifted_known >> (byte * 8u)) & 0xffu) == 0xffu)
            result.known_mask =
                (uint8_t)(result.known_mask | (1u << byte));
    return result;
}

static int equality_known(const Nba97GameCameraSelectWord* left,
    const Nba97GameCameraSelectWord* right, int* equal) {
    unsigned byte;
    for (byte = 0; byte < 4; ++byte) {
        uint8_t bit = (uint8_t)(1u << byte);
        if ((left->known_mask & bit) && (right->known_mask & bit) &&
            ((left->word >> (byte * 8u)) & 0xffu) !=
            ((right->word >> (byte * 8u)) & 0xffu)) {
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

static int64_t signed_word(uint32_t word) {
    return (word & UINT32_C(0x80000000)) ?
        (int64_t)word - INT64_C(0x100000000) : (int64_t)word;
}

static Nba97GameCameraSelectWord signed_less_immediate(
    Nba97GameCameraSelectWord source, int32_t immediate) {
    Nba97GameCameraSelectWord result;
    int64_t minimum = 0;
    int64_t maximum = 0;
    unsigned byte;
    result.word = signed_word(source.word) < immediate ? 1u : 0u;
    result.known_mask = 0x0eu;
    for (byte = 0; byte < 3; ++byte) {
        unsigned value = (source.word >> (byte * 8u)) & 0xffu;
        if (source.known_mask & (1u << byte)) {
            minimum += (int64_t)value << (byte * 8u);
            maximum += (int64_t)value << (byte * 8u);
        } else {
            maximum += INT64_C(255) << (byte * 8u);
        }
    }
    if (source.known_mask & 8u) {
        unsigned high = source.word >> 24u;
        int64_t signed_high = high >= 0x80u ?
            (int64_t)high - 0x100 : (int64_t)high;
        minimum += signed_high * INT64_C(0x1000000);
        maximum += signed_high * INT64_C(0x1000000);
    } else {
        minimum -= INT64_C(128) * INT64_C(0x1000000);
        maximum += INT64_C(127) * INT64_C(0x1000000);
    }
    if (maximum < immediate || minimum >= immediate)
        result.known_mask = 0x0fu;
    return result;
}

static int spend(Nba97GameCameraSelectRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static int require_known(Nba97GameCameraSelectRun* run,
    const Nba97GameCameraSelectWord* value, uint32_t pc,
    uint32_t address, uint32_t entry) {
    if (value->known_mask != 0x0fu) {
        stop(run, pc, address, entry);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static void journal(Nba97GameCameraSelectRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameCameraSelectWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameCameraSelectAccess* event =
            &run->context->access_journal[index];
        event->pc = pc;
        event->address = address;
        event->value = value->word;
        event->operation = run->out->operations;
        event->width = width;
        event->known_mask = (uint8_t)(value->known_mask &
            (width == 1 ? 1u : 0x0fu));
        event->kind = kind;
    }
}

static int locate(Nba97GameCameraSelectRun* run, uint32_t address,
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

static int read_value(Nba97GameCameraSelectRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    Nba97GameCameraSelectWord* value) {
    uint8_t* data;
    uint8_t* known;
    Nba97GameCameraSelectWord loaded;
    unsigned byte;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    loaded.word = 0;
    loaded.known_mask = width == 1 ? 0x0eu : 0;
    for (byte = 0; byte < width; ++byte) {
        loaded.word |= (uint32_t)data[byte] << (byte * 8u);
        if (!known || known[byte])
            loaded.known_mask =
                (uint8_t)(loaded.known_mask | (1u << byte));
    }
    *value = loaded;
    ++run->out->reads;
    journal(run, NBA97_GAME_CAMERA_SELECT_READ, pc, address,
        (uint8_t)width, &loaded);
    return NBA97_TEXT_COMPLETE;
}

static int read_word(Nba97GameCameraSelectRun* run, uint32_t address,
    uint32_t pc, Nba97GameCameraSelectWord* value) {
    return read_value(run, address, 4, 4, pc, value);
}

static int read_byte(Nba97GameCameraSelectRun* run, uint32_t address,
    uint32_t pc, Nba97GameCameraSelectWord* value) {
    return read_value(run, address, 1, 1, pc, value);
}

static int write_value(Nba97GameCameraSelectRun* run, uint32_t address,
    size_t width, size_t alignment, uint32_t pc,
    const Nba97GameCameraSelectWord* value) {
    uint8_t* data;
    uint8_t* known;
    uint8_t width_mask = width == 1 ? 1u : 0x0fu;
    unsigned byte;
    TRY(locate(run, address, width, alignment, pc, &data, &known));
    if (!known && (value->known_mask & width_mask) != width_mask)
        return NBA97_TEXT_ARGUMENT;
    for (byte = 0; byte < width; ++byte) {
        data[byte] = (uint8_t)(value->word >> (byte * 8u));
        if (known)
            known[byte] = (uint8_t)((value->known_mask >> byte) & 1u);
    }
    ++run->out->stores;
    journal(run, NBA97_GAME_CAMERA_SELECT_STORE, pc, address,
        (uint8_t)width, value);
    return NBA97_TEXT_COMPLETE;
}

static int write_word(Nba97GameCameraSelectRun* run, uint32_t address,
    uint32_t pc, const Nba97GameCameraSelectWord* value) {
    return write_value(run, address, 4, 4, pc, value);
}

static int write_byte(Nba97GameCameraSelectRun* run, uint32_t address,
    uint32_t pc, const Nba97GameCameraSelectWord* value) {
    return write_value(run, address, 1, 1, pc, value);
}

static int registers_valid(const Nba97GameCameraSelectRegisters* registers) {
    unsigned index;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (index = 0; index < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++index)
        if (registers->gpr[index].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameCameraSelectContext* context,
    Nba97GameCameraSelectProgress* out, Nba97GameCameraSelectRun* run) {
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

static Nba97GameCameraSelectWord plus_immediate(
    Nba97GameCameraSelectWord value, uint32_t amount) {
    Nba97GameCameraSelectWord immediate;
    set_known(&immediate, amount);
    return add_words(value, immediate);
}

static int dynamic_read_word(Nba97GameCameraSelectRun* run,
    Nba97GameCameraSelectWord address, uint32_t pc,
    Nba97GameCameraSelectWord* value) {
    TRY(require_known(run, &address, pc, address.word, 0));
    return read_word(run, address.word, pc, value);
}

static int dynamic_write_word(Nba97GameCameraSelectRun* run,
    Nba97GameCameraSelectWord address, uint32_t pc,
    const Nba97GameCameraSelectWord* value) {
    TRY(require_known(run, &address, pc, address.word, 0));
    return write_word(run, address.word, pc, value);
}

static int stack_read(Nba97GameCameraSelectRun* run, uint32_t offset,
    uint32_t pc, Nba97GameCameraSelectWord* value) {
    return dynamic_read_word(run, plus_immediate(
        R(NBA97_MATCH_INITIALIZE_SP), offset), pc, value);
}

static int stack_write(Nba97GameCameraSelectRun* run, uint32_t offset,
    uint32_t pc, const Nba97GameCameraSelectWord* value) {
    return dynamic_write_word(run, plus_immediate(
        R(NBA97_MATCH_INITIALIZE_SP), offset), pc, value);
}

static void jal(Nba97GameCameraSelectRun* run, uint32_t pc) {
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
}

static int invoke(Nba97GameCameraSelectRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count) {
    Nba97GameCameraSelectEvent event;
    int accepted;
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

static int nop_call(Nba97GameCameraSelectRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count) {
    jal(run, pc);
    return invoke(run, pc, entry, kind, argument_count);
}

static int branch_equal(Nba97GameCameraSelectRun* run,
    Nba97GameCameraSelectWord left, Nba97GameCameraSelectWord right,
    uint32_t pc, int* equal) {
    if (!equality_known(&left, &right, equal)) {
        stop(run, pc, 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    return NBA97_TEXT_COMPLETE;
}

static int epilogue(Nba97GameCameraSelectRun* run, uint8_t exit_kind) {
    /* 0x80079D20..0x80079D37: all three reloads use callback-live sp. */
    TRY(stack_read(run, 0x50u, UINT32_C(0x80079d20),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    run->out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
    TRY(stack_read(run, 0x4cu, UINT32_C(0x80079d24),
        &R(NBA97_MATCH_INITIALIZE_S0 + 1)));
    TRY(stack_read(run, 0x48u, UINT32_C(0x80079d28),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    R(NBA97_MATCH_INITIALIZE_SP) = plus_immediate(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0x58));
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x80079d30), 0,
            R(NBA97_MATCH_INITIALIZE_RA).word);
        return NBA97_TEXT_UNKNOWN;
    }
    run->out->stopped_pc = 0;
    run->out->stopped_address = 0;
    run->out->stopped_entry = 0;
    run->out->exit_kind = exit_kind;
    run->out->completed = 1;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int dispatch_mode(Nba97GameCameraSelectRun* run) {
    Nba97GameCameraSelectWord branch_value;
    Nba97GameCameraSelectWord constant;
    int equal;

    /* 0x80079A30..0x80079AE8: retain every signed SLTI result and branch
     * delay write before selecting the source child. */
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_immediate(
        R(NBA97_MATCH_INITIALIZE_S0), 107);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_immediate(
        R(NBA97_MATCH_INITIALIZE_S0), 100);
    set_known(&constant, 0);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079a34), &equal));
    if (equal) {
        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_immediate(
            R(NBA97_MATCH_INITIALIZE_S0), 200);
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
        set_known(&constant, 0);
        TRY(branch_equal(run, branch_value, constant,
            UINT32_C(0x80079a7c), &equal));
        if (!equal)
            return NBA97_TEXT_COMPLETE;

        R(NBA97_MATCH_INITIALIZE_V0) = signed_less_immediate(
            R(NBA97_MATCH_INITIALIZE_S0), 202);
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 11);
        set_known(&constant, 0);
        TRY(branch_equal(run, branch_value, constant,
            UINT32_C(0x80079a88), &equal));
        if (equal) {
            R(NBA97_MATCH_INITIALIZE_V0) = signed_less_immediate(
                R(NBA97_MATCH_INITIALIZE_S0), 204);
            branch_value = R(NBA97_MATCH_INITIALIZE_V0);
            set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
            set_known(&constant, 0);
            TRY(branch_equal(run, branch_value, constant,
                UINT32_C(0x80079a94), &equal));
            if (equal)
                return NBA97_TEXT_COMPLETE;
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), 10);
        }
        jal(run, UINT32_C(0x80079ae4));
        set_known(&R(NBA97_MATCH_INITIALIZE_S0), 10);
        TRY(invoke(run, UINT32_C(0x80079ae4), UINT32_C(0x8007a19c),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007A19C, 1));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
        TRY(read_word(run, UINT32_C(0x800fc99c),
            UINT32_C(0x80079af0), &R(NBA97_MATCH_INITIALIZE_V0)));
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
        TRY(branch_equal(run, branch_value,
            R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80079af8), &equal));
        if (!equal)
            return NBA97_TEXT_COMPLETE;
        TRY(nop_call(run, UINT32_C(0x80079b00),
            UINT32_C(0x8007a3a0),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007A3A0, 0));
        return epilogue(run,
            NBA97_GAME_CAMERA_SELECT_EXIT_ALREADY_SELECTED);
    }

    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 9);
    set_known(&constant, 0);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079a3c), &equal));
    if (equal) {
        jal(run, UINT32_C(0x80079ad4));
        R(NBA97_MATCH_INITIALIZE_A0) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xffffff9c));
        TRY(invoke(run, UINT32_C(0x80079ad4), UINT32_C(0x8007caf4),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007CAF4, 1));
        set_known(&R(NBA97_MATCH_INITIALIZE_S0), 9);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
        return NBA97_TEXT_COMPLETE;
    }

    branch_value = R(NBA97_MATCH_INITIALIZE_S0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_immediate(
        R(NBA97_MATCH_INITIALIZE_S0), 10);
    set_known(&constant, 9);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079a44), &equal));
    if (equal) {
        TRY(nop_call(run, UINT32_C(0x80079ac4),
            UINT32_C(0x8007cc3c),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007CC3C, 0));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
        return NBA97_TEXT_COMPLETE;
    }

    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 8);
    set_known(&constant, 0);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079a4c), &equal));
    if (!equal) {
        branch_value = R(NBA97_MATCH_INITIALIZE_S0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
        set_known(&constant, 8);
        TRY(branch_equal(run, branch_value, constant,
            UINT32_C(0x80079a54), &equal));
        if (equal) {
            TRY(nop_call(run, UINT32_C(0x80079aa4),
                UINT32_C(0x8007c964),
                NBA97_GAME_CAMERA_SELECT_CHILD_8007C964, 0));
            set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
        }
        return NBA97_TEXT_COMPLETE;
    }

    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
    branch_value = R(NBA97_MATCH_INITIALIZE_S0);
    set_known(&constant, 12);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079a68), &equal));
    if (equal) {
        TRY(nop_call(run, UINT32_C(0x80079ab4),
            UINT32_C(0x8007d3c8),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007D3C8, 0));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
    } else {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 12);
    }
    return NBA97_TEXT_COMPLETE;
}

static int common_path(Nba97GameCameraSelectRun* run) {
    Nba97GameCameraSelectWord branch_value;
    Nba97GameCameraSelectWord constant;
    Nba97GameCameraSelectWord address;
    int equal;
    unsigned iteration;

    /* 0x80079B14..0x80079B40: compute the force-camera flag. Every branch
     * delay write remains visible if byte knownness cannot decide control. */
    branch_value = R(NBA97_MATCH_INITIALIZE_S0);
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
    set_known(&constant, 12);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079b14), &equal));
    if (equal) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
        TRY(read_byte(run, UINT32_C(0x80021ed7),
            UINT32_C(0x80079b20), &R(NBA97_MATCH_INITIALIZE_V1)));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 6);
        branch_value = R(NBA97_MATCH_INITIALIZE_V1);
        set_known(&constant, 6);
        TRY(branch_equal(run, branch_value, constant,
            UINT32_C(0x80079b28), &equal));
        if (!equal)
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), 1);
        R(NBA97_MATCH_INITIALIZE_V0) = shift_left(
            R(NBA97_MATCH_INITIALIZE_S0), 2);
    } else {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 11);
        branch_value = R(NBA97_MATCH_INITIALIZE_S0);
        R(NBA97_MATCH_INITIALIZE_V0) = shift_left(
            R(NBA97_MATCH_INITIALIZE_S0), 2);
        set_known(&constant, 11);
        TRY(branch_equal(run, branch_value, constant,
            UINT32_C(0x80079b34), &equal));
        if (equal) {
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), 1);
            R(NBA97_MATCH_INITIALIZE_V0) = shift_left(
                R(NBA97_MATCH_INITIALIZE_S0), 2);
        }
    }

    /* 0x80079B44..0x80079B64: publish mode, perform the deliberately
     * unchecked wrapped table lookup, then publish flag and pointer. */
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x800fc99c),
        UINT32_C(0x80079b48), &R(NBA97_MATCH_INITIALIZE_S0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    R(NBA97_MATCH_INITIALIZE_AT) = add_words(
        R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
    address = plus_immediate(R(NBA97_MATCH_INITIALIZE_AT),
        UINT32_C(0xffffc268));
    TRY(dynamic_read_word(run, address, UINT32_C(0x80079b54),
        &R(NBA97_MATCH_INITIALIZE_V0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x800fa62c),
        UINT32_C(0x80079b5c), &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_word(run, UINT32_C(0x800fc9d0),
        UINT32_C(0x80079b64), &R(NBA97_MATCH_INITIALIZE_V0)));

    /* 0x80079B68..0x80079BD8: dispatch the update provider, and only when
     * callback-live s0 is ten copy the three live camera words and dispatch
     * 0x8007A19C with its a0=10 delay slot. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 10);
    set_known(&constant, 10);
    TRY(branch_equal(run, R(NBA97_MATCH_INITIALIZE_S0), constant,
        UINT32_C(0x80079b6c), &equal));
    if (!equal) {
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
        TRY(read_byte(run, UINT32_C(0x80021ed8),
            UINT32_C(0x80079b78), &R(NBA97_MATCH_INITIALIZE_A0)));
        TRY(nop_call(run, UINT32_C(0x80079b7c),
            UINT32_C(0x80079f78),
            NBA97_GAME_CAMERA_SELECT_CHILD_80079F78, 1));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 10);
    } else {
        jal(run, UINT32_C(0x80079b8c));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x100));
        TRY(invoke(run, UINT32_C(0x80079b8c), UINT32_C(0x80079ebc),
            NBA97_GAME_CAMERA_SELECT_CHILD_80079EBC, 1));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 10);
    }

    branch_value = R(NBA97_MATCH_INITIALIZE_S0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    set_known(&constant, 10);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079b98), &equal));
    if (equal) {
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
        TRY(read_word(run, UINT32_C(0x800fc9a0),
            UINT32_C(0x80079ba4), &R(NBA97_MATCH_INITIALIZE_V0)));
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
        TRY(read_word(run, UINT32_C(0x800fc9a4),
            UINT32_C(0x80079bac), &R(NBA97_MATCH_INITIALIZE_V1)));
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80100000));
        TRY(read_word(run, UINT32_C(0x800fc9a8),
            UINT32_C(0x80079bb4), &R(NBA97_MATCH_INITIALIZE_A1)));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
        TRY(write_word(run, UINT32_C(0x8010607c),
            UINT32_C(0x80079bbc), &R(NBA97_MATCH_INITIALIZE_V0)));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
        TRY(write_word(run, UINT32_C(0x80106080),
            UINT32_C(0x80079bc4), &R(NBA97_MATCH_INITIALIZE_V1)));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
        TRY(write_word(run, UINT32_C(0x80106084),
            UINT32_C(0x80079bcc), &R(NBA97_MATCH_INITIALIZE_A1)));
        jal(run, UINT32_C(0x80079bd0));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 10);
        TRY(invoke(run, UINT32_C(0x80079bd0), UINT32_C(0x8007a19c),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007A19C, 1));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    }

    branch_value = R(NBA97_MATCH_INITIALIZE_S0 + 1);
    R(NBA97_MATCH_INITIALIZE_A3) = plus_immediate(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0x10));
    set_known(&constant, 1);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x80079bdc), &equal));
    if (equal) {
        /* 0x80079BE4..0x80079C30: three four-load/four-store batches and
         * one two-load/two-store tail copy all fourteen words to guest stack. */
        set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x80100000));
        R(NBA97_MATCH_INITIALIZE_A2) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0xffffc99c));
        R(NBA97_MATCH_INITIALIZE_T0) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x30));
        for (iteration = 0; iteration < 3; ++iteration) {
            TRY(dynamic_read_word(run, R(NBA97_MATCH_INITIALIZE_A2),
                UINT32_C(0x80079bf0), &R(NBA97_MATCH_INITIALIZE_V0)));
            TRY(dynamic_read_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), 4), UINT32_C(0x80079bf4),
                &R(NBA97_MATCH_INITIALIZE_V1)));
            TRY(dynamic_read_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), 8), UINT32_C(0x80079bf8),
                &R(NBA97_MATCH_INITIALIZE_A0)));
            TRY(dynamic_read_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), 12), UINT32_C(0x80079bfc),
                &R(NBA97_MATCH_INITIALIZE_A1)));
            TRY(dynamic_write_word(run, R(NBA97_MATCH_INITIALIZE_A3),
                UINT32_C(0x80079c00), &R(NBA97_MATCH_INITIALIZE_V0)));
            TRY(dynamic_write_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), 4), UINT32_C(0x80079c04),
                &R(NBA97_MATCH_INITIALIZE_V1)));
            TRY(dynamic_write_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), 8), UINT32_C(0x80079c08),
                &R(NBA97_MATCH_INITIALIZE_A0)));
            TRY(dynamic_write_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), 12), UINT32_C(0x80079c0c),
                &R(NBA97_MATCH_INITIALIZE_A1)));
            R(NBA97_MATCH_INITIALIZE_A2) = plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x10));
            R(NBA97_MATCH_INITIALIZE_A3) = plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x10));
        }
        TRY(dynamic_read_word(run, R(NBA97_MATCH_INITIALIZE_A2),
            UINT32_C(0x80079c1c), &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(dynamic_read_word(run, plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A2), 4), UINT32_C(0x80079c20),
            &R(NBA97_MATCH_INITIALIZE_V1)));
        TRY(dynamic_write_word(run, R(NBA97_MATCH_INITIALIZE_A3),
            UINT32_C(0x80079c24), &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(dynamic_write_word(run, plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A3), 4), UINT32_C(0x80079c28),
            &R(NBA97_MATCH_INITIALIZE_V1)));
        jal(run, UINT32_C(0x80079c2c));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffffff));
        TRY(invoke(run, UINT32_C(0x80079c2c), UINT32_C(0x800798b4),
            NBA97_GAME_CAMERA_SELECT_CHILD_800798B4, 1));

        /* 0x80079C34..0x80079C80: rebuild cursors from callback-live sp and
         * restore all fourteen words; the last store is the J delay slot. */
        set_known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80100000));
        R(NBA97_MATCH_INITIALIZE_A3) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0xffffc99c));
        R(NBA97_MATCH_INITIALIZE_A2) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0x10));
        R(NBA97_MATCH_INITIALIZE_T0) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0x40));
        for (iteration = 0; iteration < 3; ++iteration) {
            TRY(dynamic_read_word(run, R(NBA97_MATCH_INITIALIZE_A2),
                UINT32_C(0x80079c44), &R(NBA97_MATCH_INITIALIZE_V0)));
            TRY(dynamic_read_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), 4), UINT32_C(0x80079c48),
                &R(NBA97_MATCH_INITIALIZE_V1)));
            TRY(dynamic_read_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), 8), UINT32_C(0x80079c4c),
                &R(NBA97_MATCH_INITIALIZE_A0)));
            TRY(dynamic_read_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), 12), UINT32_C(0x80079c50),
                &R(NBA97_MATCH_INITIALIZE_A1)));
            TRY(dynamic_write_word(run, R(NBA97_MATCH_INITIALIZE_A3),
                UINT32_C(0x80079c54), &R(NBA97_MATCH_INITIALIZE_V0)));
            TRY(dynamic_write_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), 4), UINT32_C(0x80079c58),
                &R(NBA97_MATCH_INITIALIZE_V1)));
            TRY(dynamic_write_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), 8), UINT32_C(0x80079c5c),
                &R(NBA97_MATCH_INITIALIZE_A0)));
            TRY(dynamic_write_word(run, plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), 12), UINT32_C(0x80079c60),
                &R(NBA97_MATCH_INITIALIZE_A1)));
            R(NBA97_MATCH_INITIALIZE_A2) = plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x10));
            R(NBA97_MATCH_INITIALIZE_A3) = plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x10));
        }
        TRY(dynamic_read_word(run, R(NBA97_MATCH_INITIALIZE_A2),
            UINT32_C(0x80079c70), &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(dynamic_read_word(run, plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A2), 4), UINT32_C(0x80079c74),
            &R(NBA97_MATCH_INITIALIZE_V1)));
        TRY(dynamic_write_word(run, R(NBA97_MATCH_INITIALIZE_A3),
            UINT32_C(0x80079c78), &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(dynamic_write_word(run, plus_immediate(
            R(NBA97_MATCH_INITIALIZE_A3), 4), UINT32_C(0x80079c80),
            &R(NBA97_MATCH_INITIALIZE_V1)));
    } else {
        set_known(&constant, 0);
        TRY(branch_equal(run, R(NBA97_MATCH_INITIALIZE_S0 + 1), constant,
            UINT32_C(0x80079c84), &equal));
        if (equal) {
            jal(run, UINT32_C(0x80079c8c));
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffffff));
            TRY(invoke(run, UINT32_C(0x80079c8c),
                UINT32_C(0x800798b4),
                NBA97_GAME_CAMERA_SELECT_CHILD_800798B4, 1));
            set_known(&R(NBA97_MATCH_INITIALIZE_A1),
                UINT32_C(0x80110000));
            R(NBA97_MATCH_INITIALIZE_A1) = plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0xffff9aa8));
            set_known(&R(NBA97_MATCH_INITIALIZE_A0),
                UINT32_C(0x80100000));
            R(NBA97_MATCH_INITIALIZE_A0) = plus_immediate(
                R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffc9a0));
            for (iteration = 0; iteration < 3; ++iteration) {
                uint32_t source_offset = iteration * 8u;
                uint32_t destination_offset = iteration * 8u;
                uint32_t load_pc = UINT32_C(0x80079ca4) + iteration * 16u;
                uint32_t store_pc = UINT32_C(0x80079cac) + iteration * 16u;
                TRY(dynamic_read_word(run, plus_immediate(
                    R(NBA97_MATCH_INITIALIZE_A1), source_offset), load_pc,
                    &R(NBA97_MATCH_INITIALIZE_V0)));
                TRY(dynamic_read_word(run, plus_immediate(
                    R(NBA97_MATCH_INITIALIZE_A1), source_offset + 4u),
                    load_pc + 4u, &R(NBA97_MATCH_INITIALIZE_V1)));
                TRY(dynamic_write_word(run, plus_immediate(
                    R(NBA97_MATCH_INITIALIZE_A0), destination_offset),
                    store_pc, &R(NBA97_MATCH_INITIALIZE_V0)));
                TRY(dynamic_write_word(run, plus_immediate(
                    R(NBA97_MATCH_INITIALIZE_A0), destination_offset + 4u),
                    store_pc + 4u, &R(NBA97_MATCH_INITIALIZE_V1)));
            }
            set_known(&constant, 0);
            for (iteration = 0; iteration < 6; ++iteration) {
                set_known(&R(NBA97_MATCH_INITIALIZE_AT),
                    UINT32_C(0x80100000));
                TRY(write_word(run, UINT32_C(0x800fc9b8) + iteration * 4u,
                    UINT32_C(0x80079cd8) + iteration * 8u, &constant));
            }
        }
    }

    /* 0x80079D04..0x80079D1C: normal return clears busy, refreshes the
     * camera provider, and publishes -1 after that callback. */
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    set_known(&constant, 0);
    TRY(write_byte(run, UINT32_C(0x801029f8),
        UINT32_C(0x80079d08), &constant));
    TRY(nop_call(run, UINT32_C(0x80079d0c),
        UINT32_C(0x8007a3a0),
        NBA97_GAME_CAMERA_SELECT_CHILD_8007A3A0, 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    TRY(write_word(run, UINT32_C(0x800bc1f4),
        UINT32_C(0x80079d1c), &R(NBA97_MATCH_INITIALIZE_V0)));
    return epilogue(run, NBA97_GAME_CAMERA_SELECT_EXIT_NORMAL);
}

int nba97_game_camera_select(Nba97GameCameraSelectContext* context,
    Nba97GameCameraSelectProgress* out) {
    Nba97GameCameraSelectRun storage;
    Nba97GameCameraSelectRun* run = &storage;
    Nba97GameCameraSelectWord branch_value;
    Nba97GameCameraSelectWord constant;
    int equal;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x800799CC..0x800799E4: form the 0x58-byte frame and preserve
     * s0/s1/ra in exact order; the ra store is the entry branch delay slot. */
    R(NBA97_MATCH_INITIALIZE_SP) = plus_immediate(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffa8));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(stack_write(run, 0x48u, UINT32_C(0x800799d0),
        &R(NBA97_MATCH_INITIALIZE_S0)));
    R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A0);
    TRY(stack_write(run, 0x4cu, UINT32_C(0x800799d8),
        &R(NBA97_MATCH_INITIALIZE_S0 + 1)));
    R(NBA97_MATCH_INITIALIZE_S0 + 1) = R(NBA97_MATCH_INITIALIZE_A1);
    branch_value = R(NBA97_MATCH_INITIALIZE_S0);
    TRY(stack_write(run, 0x50u, UINT32_C(0x800799e4),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    set_known(&constant, 0);
    TRY(branch_equal(run, branch_value, constant,
        UINT32_C(0x800799e0), &equal));

    if (equal) {
        /* 0x800799E8..0x80079A20: clear a previous nonzero mode before the
         * child and always publish live s0 (zero) before the early return. */
        set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
        R(NBA97_MATCH_INITIALIZE_V1) = plus_immediate(
            R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffc99c));
        TRY(dynamic_read_word(run, R(NBA97_MATCH_INITIALIZE_V1),
            UINT32_C(0x800799f0), &R(NBA97_MATCH_INITIALIZE_V0)));
        branch_value = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 1);
        set_known(&constant, 0);
        TRY(branch_equal(run, branch_value, constant,
            UINT32_C(0x800799f8), &equal));
        if (!equal) {
            set_known(&constant, 0);
            TRY(dynamic_write_word(run, R(NBA97_MATCH_INITIALIZE_V1),
                UINT32_C(0x80079a04), &constant));
        } else {
            set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
        }
        TRY(nop_call(run, UINT32_C(0x80079a0c),
            UINT32_C(0x8007e26c),
            NBA97_GAME_CAMERA_SELECT_CHILD_8007E26C, 1));
        set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
        TRY(write_word(run, UINT32_C(0x800fc99c),
            UINT32_C(0x80079a18), &R(NBA97_MATCH_INITIALIZE_S0)));
        return epilogue(run, NBA97_GAME_CAMERA_SELECT_EXIT_MODE_ZERO);
    }

    /* 0x80079A24..0x80079A2C: nonzero modes publish the busy byte before
     * any signed dispatch or child call. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_byte(run, UINT32_C(0x801029f8),
        UINT32_C(0x80079a2c), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(dispatch_mode(run));
    if (out->completed)
        return NBA97_TEXT_COMPLETE;
    return common_path(run);
}
