#include "game_tipoff_announcement.h"

#include <limits.h>
#include <string.h>

#define MODE_ADDRESS UINT32_C(0x80021d70)
#define MODE_TWO_FIRST_ADDRESS UINT32_C(0x80021d74)
#define MODE_TWO_SECOND_ADDRESS UINT32_C(0x80021d78)
#define MODE_ONE_FLAG_ADDRESS UINT32_C(0x8001ec94)

typedef struct Nba97GameTipoffAnnouncementRun {
    Nba97GameTipoffAnnouncementContext* context;
    Nba97GameTipoffAnnouncementProgress* out;
    Nba97GameTipoffAnnouncementRegisters registers;
} Nba97GameTipoffAnnouncementRun;

#define R(index) (run->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameTipoffAnnouncementRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameTipoffAnnouncementRun* run, uint32_t pc,
    uint32_t address, uint32_t entry) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    run->out->stopped_entry = entry;
    publish(run);
}

static int registers_valid(
    const Nba97GameTipoffAnnouncementRegisters* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static int validate(Nba97GameTipoffAnnouncementContext* context,
    Nba97GameTipoffAnnouncementProgress* out,
    Nba97GameTipoffAnnouncementRun* run) {
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

static void set_known(Nba97GameTipoffAnnouncementWord* value,
    uint32_t word) {
    value->word = word;
    value->known_mask = 0x0fu;
}

/* Evaluate all possible source-byte and carry values. This retains knowledge
 * only when every value represented by the two operands yields the same byte. */
static Nba97GameTipoffAnnouncementWord add_words(
    Nba97GameTipoffAnnouncementWord left,
    Nba97GameTipoffAnnouncementWord right) {
    Nba97GameTipoffAnnouncementWord result;
    unsigned carry_mask = 1u;
    unsigned byte;
    result.word = left.word + right.word;
    result.known_mask = 0;
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

static Nba97GameTipoffAnnouncementWord add_constant(
    Nba97GameTipoffAnnouncementWord source, uint32_t constant) {
    Nba97GameTipoffAnnouncementWord addend;
    set_known(&addend, constant);
    return add_words(source, addend);
}

static int spend(Nba97GameTipoffAnnouncementRun* run) {
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
    return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
    return width == 4 ? UINT32_MAX :
        (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
    return (uint8_t)((1u << width) - 1u);
}

static void journal(Nba97GameTipoffAnnouncementRun* run, uint8_t kind,
    uint32_t pc, uint32_t address, uint8_t width,
    const Nba97GameTipoffAnnouncementWord* value) {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
        Nba97GameTipoffAnnouncementAccess* event =
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

static int locate(Nba97GameTipoffAnnouncementRun* run, uint32_t address,
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

static int read_value(Nba97GameTipoffAnnouncementRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    Nba97GameTipoffAnnouncementWord* value) {
    Nba97GameTipoffAnnouncementWord loaded = {0, 0};
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
    journal(run, NBA97_GAME_TIPOFF_ANNOUNCEMENT_READ, pc, address, width,
        value);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int write_value(Nba97GameTipoffAnnouncementRun* run,
    uint32_t address, uint8_t width, uint32_t pc,
    const Nba97GameTipoffAnnouncementWord* value) {
    Nba97GameTipoffAnnouncementWord stored = *value;
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
    journal(run, NBA97_GAME_TIPOFF_ANNOUNCEMENT_STORE, pc, address, width,
        &stored);
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

static int effective_stack_address(Nba97GameTipoffAnnouncementRun* run,
    uint32_t offset, uint32_t pc, uint32_t* address) {
    Nba97GameTipoffAnnouncementWord value = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), offset);
    if (value.known_mask != 0x0fu) {
        stop(run, pc, value.word, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    *address = value.word;
    return NBA97_TEXT_COMPLETE;
}

static Nba97GameTipoffAnnouncementWord zero_extend_byte(
    Nba97GameTipoffAnnouncementWord source) {
    Nba97GameTipoffAnnouncementWord result;
    result.word = source.word & 0xffu;
    result.known_mask = (uint8_t)((source.known_mask & 1u) | 0x0eu);
    return result;
}

static int64_t signed_word(uint32_t word) {
    return word < UINT32_C(0x80000000) ? (int64_t)word :
        (int64_t)word - INT64_C(0x100000000);
}

static void signed_bounds(const Nba97GameTipoffAnnouncementWord* value,
    int64_t* minimum, int64_t* maximum) {
    uint32_t low = 0;
    uint32_t high = 0;
    unsigned i;
    if (!(value->known_mask & 8u)) {
        *minimum = INT32_MIN;
        *maximum = INT32_MAX;
        return;
    }
    for (i = 0; i < 4; ++i) {
        uint32_t byte = (value->word >> (i * 8u)) & 0xffu;
        low |= ((value->known_mask & (1u << i)) ? byte : 0u) << (i * 8u);
        high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) <<
            (i * 8u);
    }
    *minimum = signed_word(low);
    *maximum = signed_word(high);
}

static Nba97GameTipoffAnnouncementWord signed_less_than_constant(
    const Nba97GameTipoffAnnouncementWord* value, int32_t constant) {
    Nba97GameTipoffAnnouncementWord result;
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    result.word = signed_word(value->word) < constant;
    result.known_mask = 0x0eu; /* SLTI always produces zero upper bytes. */
    if (maximum < constant)
        set_known(&result, 1);
    else if (minimum >= constant)
        set_known(&result, 0);
    return result;
}

static int decide_zero(Nba97GameTipoffAnnouncementRun* run,
    const Nba97GameTipoffAnnouncementWord* value, uint32_t pc,
    int* is_zero) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        if ((value->known_mask & (1u << i)) &&
            ((value->word >> (i * 8u)) & 0xffu)) {
            *is_zero = 0;
            return NBA97_TEXT_COMPLETE;
        }
    if (value->known_mask == 0x0fu) {
        *is_zero = 1;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int decide_equal_constant(Nba97GameTipoffAnnouncementRun* run,
    const Nba97GameTipoffAnnouncementWord* value, uint32_t constant,
    uint32_t pc, int* equal) {
    unsigned i;
    for (i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((value->known_mask & bit) &&
            ((value->word >> (i * 8u)) & 0xffu) !=
            ((constant >> (i * 8u)) & 0xffu)) {
            *equal = 0;
            return NBA97_TEXT_COMPLETE;
        }
    }
    if (value->known_mask == 0x0fu) {
        *equal = value->word == constant;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

static int decide_positive(Nba97GameTipoffAnnouncementRun* run,
    const Nba97GameTipoffAnnouncementWord* value, uint32_t pc,
    int* positive) {
    int64_t minimum;
    int64_t maximum;
    signed_bounds(value, &minimum, &maximum);
    if (minimum > 0) {
        *positive = 1;
        return NBA97_TEXT_COMPLETE;
    }
    if (maximum <= 0) {
        *positive = 0;
        return NBA97_TEXT_COMPLETE;
    }
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
}

enum Nba97GameTipoffAnnouncementDelay {
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_NOP = 0,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A0_ZERO,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_ZERO,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_ONE,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S2_V0,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S1_V0,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S0_V0,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S1_ADD_V0,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_S2,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A2_S1,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A3_S2,
    NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_SAVE_S0
};

static int invoke(Nba97GameTipoffAnnouncementRun* run, uint32_t pc,
    uint32_t entry, uint8_t kind, uint8_t argument_count,
    enum Nba97GameTipoffAnnouncementDelay delay) {
    Nba97GameTipoffAnnouncementEvent event;
    uint32_t address;
    int accepted;
    set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
    switch (delay) {
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A0_ZERO:
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_ZERO:
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_ONE:
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 1);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S2_V0:
        R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2) = R(NBA97_MATCH_INITIALIZE_V0);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S1_V0:
        R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1) = R(NBA97_MATCH_INITIALIZE_V0);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S0_V0:
        R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_V0);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S1_ADD_V0:
        R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1) = add_words(
            R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1),
            R(NBA97_MATCH_INITIALIZE_V0));
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_S2:
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A2_S1:
        R(NBA97_MATCH_INITIALIZE_A2) = R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A3_S2:
        R(NBA97_MATCH_INITIALIZE_A3) = R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2);
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_SAVE_S0:
        TRY(effective_stack_address(run, 0x10u, pc + 4u, &address));
        TRY(write_value(run, address, 4, pc + 4u,
            &R(NBA97_MATCH_INITIALIZE_S0)));
        break;
    case NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_NOP:
        break;
    }
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

static int restore(Nba97GameTipoffAnnouncementRun* run, uint32_t pc,
    uint32_t offset, unsigned reg,
    Nba97GameTipoffAnnouncementWord* reported) {
    uint32_t address;
    TRY(effective_stack_address(run, offset, pc, &address));
    TRY(read_value(run, address, 4, pc, &R(reg)));
    *reported = R(reg);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_tipoff_announcement(
    Nba97GameTipoffAnnouncementContext* context,
    Nba97GameTipoffAnnouncementProgress* out) {
    Nba97GameTipoffAnnouncementRun storage;
    Nba97GameTipoffAnnouncementRun* run = &storage;
    Nba97GameTipoffAnnouncementWord value;
    uint32_t address;
    int branch;
    int decision_result;
    TRY(validate(context, out, run));

    /* GAMEONLY 0x8007EF4C..0x8007EF60: create the frame. JAL writes ra
     * before the source delay-slot store, so a failed store retains new ra. */
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
    out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
    TRY(effective_stack_address(run, 0x1cu, UINT32_C(0x8007ef50), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8007ef50),
        &R(NBA97_MATCH_INITIALIZE_RA)));
    TRY(effective_stack_address(run, 0x18u, UINT32_C(0x8007ef54), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8007ef54),
        &R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2)));
    TRY(effective_stack_address(run, 0x14u, UINT32_C(0x8007ef58), &address));
    TRY(write_value(run, address, 4, UINT32_C(0x8007ef58),
        &R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1)));
    TRY(invoke(run, UINT32_C(0x8007ef5c), UINT32_C(0x800887e8),
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_800887E8, 0,
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_SAVE_S0));

    /* 0x8007EF64..0x8007EF88: signed gate results preserve the SLTI upper
     * three known-zero bytes. Both branch delay slots execute before refusal. */
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_than_constant(
        &R(NBA97_MATCH_INITIALIZE_V0), 8);
    out->gate = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
        UINT32_C(0x8007ef68), &branch));
    if (!branch)
        goto restore_registers;
    TRY(invoke(run, UINT32_C(0x8007ef70), UINT32_C(0x8007fa50),
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA50, 1,
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A0_ZERO));
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
    TRY(read_value(run, MODE_ADDRESS, 1, UINT32_C(0x8007ef7c), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = zero_extend_byte(value);
    out->mode = R(NBA97_MATCH_INITIALIZE_V1);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
    decision_result = decide_equal_constant(run,
        &R(NBA97_MATCH_INITIALIZE_V1), 2,
        UINT32_C(0x8007ef84), &branch);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1); /* branch delay */
    if (decision_result != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x8007ef84), 0, 0);
        return decision_result;
    }

    if (branch) {
        /* 0x8007EF8C..0x8007F008: mode 2 builds both announcement halves.
         * Captures and the wrapping sum occur in their source JAL delays. */
        out->mode_path = 2;
        TRY(invoke(run, UINT32_C(0x8007ef8c), UINT32_C(0x8007eea8),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007EEA8, 0,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_NOP));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
        TRY(invoke(run, UINT32_C(0x8007ef98), UINT32_C(0x8007fa9c),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA9C, 1,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S2_V0));
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_V0);
        TRY(invoke(run, UINT32_C(0x8007efa4), UINT32_C(0x8007eca4),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007ECA4, 2,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_S2));
        TRY(invoke(run, UINT32_C(0x8007efac), UINT32_C(0x800b1e14),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_800B1E14, 0,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_NOP));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
        TRY(read_value(run, MODE_TWO_FIRST_ADDRESS, 4,
            UINT32_C(0x8007efb8), &R(NBA97_MATCH_INITIALIZE_A0)));
        TRY(invoke(run, UINT32_C(0x8007efbc), UINT32_C(0x80083748),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_80083748, 2,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A1_ZERO));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
        TRY(read_value(run, MODE_TWO_SECOND_ADDRESS, 4,
            UINT32_C(0x8007efc8), &R(NBA97_MATCH_INITIALIZE_A0)));
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 1);
        TRY(invoke(run, UINT32_C(0x8007efd0), UINT32_C(0x80083748),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_80083748, 2,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S1_V0));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
        TRY(invoke(run, UINT32_C(0x8007efdc), UINT32_C(0x8007fa9c),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA9C, 1,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S1_ADD_V0));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 5);
        TRY(invoke(run, UINT32_C(0x8007efe8), UINT32_C(0x8007fa9c),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA9C, 1,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S0_V0));
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_A2) = R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1);
        TRY(invoke(run, UINT32_C(0x8007effc), UINT32_C(0x8007ecec),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007ECEC, 4,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A3_S2));
    } else {
        /* 0x8007F00C..0x8007F04C: S1=3 is unconditional. Only mode 1 reads
         * the signed flag, and a strictly positive value replaces it with 5. */
        decision_result = decide_equal_constant(run,
            &R(NBA97_MATCH_INITIALIZE_V1), 1,
            UINT32_C(0x8007f00c), &branch);
        set_known(&R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1), 3); /* delay */
        if (decision_result != NBA97_TEXT_COMPLETE) {
            stop(run, UINT32_C(0x8007f00c), 0, 0);
            return decision_result;
        }
        if (branch) {
            out->mode_path = 1;
            set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
            TRY(read_value(run, MODE_ONE_FLAG_ADDRESS, 4,
                UINT32_C(0x8007f018), &R(NBA97_MATCH_INITIALIZE_V0)));
            TRY(decide_positive(run, &R(NBA97_MATCH_INITIALIZE_V0),
                UINT32_C(0x8007f020), &branch));
            if (branch)
                set_known(&R(NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1), 5);
        }
        TRY(invoke(run, UINT32_C(0x8007f02c), UINT32_C(0x8007fa9c),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA9C, 1,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A0_ZERO));
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 5);
        TRY(invoke(run, UINT32_C(0x8007f038), UINT32_C(0x8007fa9c),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007FA9C, 1,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_S0_V0));
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_V0);
        TRY(invoke(run, UINT32_C(0x8007f048), UINT32_C(0x8007e8c4),
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_8007E8C4, 3,
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_A2_S1));
    }
    TRY(invoke(run, UINT32_C(0x8007f050), UINT32_C(0x800b1e14),
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_CHILD_800B1E14, 0,
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_DELAY_NOP));

restore_registers:
    /* 0x8007F058..0x8007F070: all four loads use child-mutable live sp and
     * preserve read order before ADDIU and the possibly unknown JR target. */
    TRY(restore(run, UINT32_C(0x8007f058), 0x1cu,
        NBA97_MATCH_INITIALIZE_RA, &out->restored_return_address));
    TRY(restore(run, UINT32_C(0x8007f05c), 0x18u,
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2, &out->restored_s2));
    TRY(restore(run, UINT32_C(0x8007f060), 0x14u,
        NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1, &out->restored_s1));
    TRY(restore(run, UINT32_C(0x8007f064), 0x10u,
        NBA97_MATCH_INITIALIZE_S0, &out->restored_s0));
    R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
        R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
    if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8007f06c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
