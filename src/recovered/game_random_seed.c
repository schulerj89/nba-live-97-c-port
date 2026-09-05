#include "game_random_seed.h"

#include <string.h>

typedef Nba97GameRandomSeedWord Word;
typedef Nba97GameRandomSeedRegisters Registers;

typedef struct Nba97GameRandomSeedRun {
    Nba97GameRandomSeedContext* context;
    Nba97GameRandomSeedProgress* out;
    Registers registers;
} Nba97GameRandomSeedRun;

#define REG(run, index) ((run)->registers.gpr[(index)])
#define TRY(expression) do { \
    int nba97_result_ = (expression); \
    if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Nba97GameRandomSeedRun* run) {
    run->out->registers = run->registers;
}

static void stop(Nba97GameRandomSeedRun* run, uint32_t pc,
    uint32_t address) {
    run->out->stopped_pc = pc;
    run->out->stopped_address = address;
    publish(run);
}

static int registers_valid(const Registers* registers) {
    unsigned i;
    if (registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu)
        return 0;
    for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (registers->gpr[i].known_mask > 0x0fu)
            return 0;
    return 1;
}

static void constant(Word* target, uint32_t value) {
    target->word = value;
    target->known_mask = 0x0fu;
}

/* Add one fully known word while retaining exactly the byte knowledge that
 * survives the source ADDU carry chain. */
static Word add_constant(Word source, uint32_t addend) {
    const uint32_t original = source.word;
    const uint8_t original_known = source.known_mask;
    uint8_t result_known = 0;
    unsigned carry = 0;
    unsigned carry_known = 1;
    unsigned i;
    source.word += addend;
    for (i = 0; i < 4; ++i) {
        const unsigned byte_known = (original_known >> i) & 1u;
        const unsigned byte = (original >> (i * 8u)) & 0xffu;
        const unsigned add_byte = (addend >> (i * 8u)) & 0xffu;
        if (byte_known && carry_known)
            result_known = (uint8_t)(result_known | (uint8_t)(1u << i));
        if (byte_known && carry_known) {
            carry = byte + add_byte + carry > 0xffu;
        } else if (!byte_known && carry_known && add_byte + carry == 0u) {
            carry = 0;
            carry_known = 1;
        } else if (!byte_known && carry_known &&
            add_byte + carry == 0x100u) {
            carry = 1;
            carry_known = 1;
        } else if (byte_known && !carry_known &&
            byte + add_byte != 0xffu) {
            carry = byte + add_byte > 0xffu;
            carry_known = 1;
        } else {
            carry_known = 0;
        }
    }
    source.known_mask = result_known;
    return source;
}

static int validate(Nba97GameRandomSeedContext* context,
    Nba97GameRandomSeedProgress* out, Nba97GameRandomSeedRun* run) {
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

static int locate_store(Nba97GameRandomSeedRun* run, uint32_t address,
    uint32_t pc, uint8_t** data, uint8_t** known) {
    size_t i;
    size_t j;
    stop(run, pc, address);
    if (run->out->operations >= run->context->operation_budget)
        return NBA97_TEXT_LIMIT;
    ++run->out->operations;
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

static int write_word(Nba97GameRandomSeedRun* run, uint32_t address,
    uint32_t pc, const Word* value) {
    Nba97GameRandomSeedAccess event;
    uint8_t* data;
    uint8_t* known;
    size_t index;
    unsigned i;
    TRY(locate_store(run, address, pc, &data, &known));
    if (!known && value->known_mask != 0x0fu)
        return NBA97_TEXT_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value->word >> (i * 8u));
        if (known)
            known[i] = (uint8_t)((value->known_mask >> i) & 1u);
    }
    ++run->out->stores;
    event.pc = pc;
    event.address = address;
    event.value = value->word;
    event.operation = run->out->operations;
    event.width = 4;
    event.known_mask = value->known_mask;
    event.kind = NBA97_GAME_RANDOM_SEED_STORE;
    index = run->out->access_events++;
    if (index < run->context->access_journal_capacity)
        run->context->access_journal[index] = event;
    publish(run);
    return NBA97_TEXT_COMPLETE;
}

int nba97_game_random_seed(Nba97GameRandomSeedContext* context,
    Nba97GameRandomSeedProgress* out) {
    Nba97GameRandomSeedRun storage;
    Nba97GameRandomSeedRun* run = &storage;
    Word* at;
    Word* v0;
    Word* a0;
    TRY(validate(context, out, run));
    at = &REG(run, NBA97_MATCH_INITIALIZE_AT);
    v0 = &REG(run, NBA97_MATCH_INITIALIZE_V0);
    a0 = &REG(run, NBA97_MATCH_INITIALIZE_A0);

    /* GAMEONLY 0x80093694..0x80093698: form the fixed six-word destination
     * base in a1; the leaf never derives a native pointer from it. */
    constant(&REG(run, NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x800c0000));
    REG(run, NBA97_MATCH_INITIALIZE_A1) = add_constant(
        REG(run, NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x00004ae8));

    /* 0x8009369C LUI v0; 0x800936A0 LUI at; 0x800936A4 ORI at;
     * 0x800936A8 signed ADD is safe for these fixed negative operands. */
    constant(v0, UINT32_C(0xf22d0000));
    constant(at, UINT32_C(0xf22d0000));
    at->word |= UINT32_C(0x00000e56);
    v0->word += at->word;
    *a0 = add_constant(*a0, v0->word); /* 0x800936AC ADDU */
    TRY(write_word(run, UINT32_C(0x800c4ae8), UINT32_C(0x800936b0), a0));

    /* GAMEONLY 0x800936B4..0x800936C8. */
    constant(v0, UINT32_C(0x96040000));
    constant(at, UINT32_C(0x96040000));
    at->word |= UINT32_C(0x00001893);
    v0->word += at->word;
    *a0 = add_constant(*a0, v0->word);
    TRY(write_word(run, UINT32_C(0x800c4aec), UINT32_C(0x800936c8), a0));

    /* GAMEONLY 0x800936CC..0x800936E0. */
    constant(v0, UINT32_C(0x3df30000));
    constant(at, UINT32_C(0x3df30000));
    at->word |= UINT32_C(0x0000b646);
    v0->word += at->word;
    *a0 = add_constant(*a0, v0->word);
    TRY(write_word(run, UINT32_C(0x800c4af0), UINT32_C(0x800936e0), a0));

    /* GAMEONLY 0x800936E4..0x800936F8. */
    constant(v0, UINT32_C(0x40dd0000));
    constant(at, UINT32_C(0x40dd0000));
    at->word |= UINT32_C(0x0000e76d);
    v0->word += at->word;
    *a0 = add_constant(*a0, v0->word);
    TRY(write_word(run, UINT32_C(0x800c4af4), UINT32_C(0x800936f8), a0));

    /* GAMEONLY 0x800936FC..0x80093710. */
    constant(v0, UINT32_C(0x97320000));
    constant(at, UINT32_C(0x97320000));
    at->word |= UINT32_C(0x00007ae1);
    v0->word += at->word;
    *a0 = add_constant(*a0, v0->word);
    TRY(write_word(run, UINT32_C(0x800c4af8), UINT32_C(0x80093710), a0));

    /* GAMEONLY 0x80093714..0x80093728. */
    constant(v0, UINT32_C(0xd1a90000));
    constant(at, UINT32_C(0xd1a90000));
    at->word |= UINT32_C(0x0000fbe7);
    v0->word += at->word;
    *a0 = add_constant(*a0, v0->word);
    TRY(write_word(run, UINT32_C(0x800c4afc), UINT32_C(0x80093728), a0));

    /* 0x8009372C JR ra; 0x80093730 NOP. The branch target is consumed only
     * after the complete six-store prefix. */
    if (REG(run, NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
        stop(run, UINT32_C(0x8009372c), 0);
        return NBA97_TEXT_UNKNOWN;
    }
    out->completed = 1;
    stop(run, 0, 0);
    return NBA97_TEXT_COMPLETE;
}
