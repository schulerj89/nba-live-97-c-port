#include "recovered/game_random_seed.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game random seed check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Base = 0x800c4ae8u;
constexpr std::array<std::uint32_t, 6> Increment{{
    0xe45a0e56u, 0x2c081893u, 0x7be6b646u,
    0x81bae76du, 0x2e647ae1u, 0xa352fbe7u}};
constexpr std::array<std::uint32_t, 6> OperandAt{{
    0xf22d0e56u, 0x96041893u, 0x3df3b646u,
    0x40dde76du, 0x97327ae1u, 0xd1a9fbe7u}};
constexpr std::array<std::uint32_t, 6> StorePc{{
    0x800936b0u, 0x800936c8u, 0x800936e0u,
    0x800936f8u, 0x80093710u, 0x80093728u}};

struct RefWord {
    std::uint32_t word;
    std::uint8_t mask;
};

RefWord add_ref(RefWord source, std::uint32_t addend) {
    unsigned carry_set = 1u; // bit zero means carry=0 is possible.
    std::uint8_t known = 0;
    for (unsigned byte_index = 0; byte_index < 4; ++byte_index) {
        bool output[256]{};
        unsigned output_count = 0;
        unsigned next_carry_set = 0;
        const bool byte_known = (source.mask & (1u << byte_index)) != 0;
        const unsigned actual_byte =
            (source.word >> (8u * byte_index)) & 0xffu;
        const unsigned add_byte = (addend >> (8u * byte_index)) & 0xffu;
        for (unsigned carry = 0; carry <= 1; ++carry) {
            if (!(carry_set & (1u << carry)))
                continue;
            const unsigned first = byte_known ? actual_byte : 0;
            const unsigned last = byte_known ? actual_byte : 255;
            for (unsigned input = first; input <= last; ++input) {
                const unsigned sum = input + add_byte + carry;
                if (!output[sum & 0xffu]) {
                    output[sum & 0xffu] = true;
                    ++output_count;
                }
                next_carry_set |= 1u << (sum >> 8u);
            }
        }
        if (output_count == 1)
            known = static_cast<std::uint8_t>(known | (1u << byte_index));
        carry_set = next_carry_set;
    }
    return {source.word + addend, known};
}

std::array<RefWord, 6> expected(std::uint32_t seed, std::uint8_t mask) {
    std::array<RefWord, 6> result{};
    RefWord value{seed, mask};
    for (unsigned i = 0; i < result.size(); ++i) {
        value = add_ref(value, Increment[i]);
        result[i] = value;
    }
    return result;
}

struct Fixture {
    std::array<std::uint8_t, 24> bytes{};
    std::array<std::uint8_t, 24> known{};
    Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
    std::array<Nba97GameRandomSeedAccess, 6> journal{};
    Nba97GameRandomSeedContext context{};
    Nba97GameRandomSeedProgress progress{};

    Fixture(std::uint32_t seed = 0, std::uint8_t seed_mask = 0x0f) {
        bytes.fill(0xcd);
        known.fill(1);
        context.memory = {&region, 1};
        context.operation_budget = 6;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x51000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {seed, seed_mask};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x800802d8u, 0x0f};
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }

    std::uint32_t get(unsigned index) const {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[index * 4u + i]) << (8u * i);
        return value;
    }

    std::uint8_t get_known(unsigned index) const {
        std::uint8_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value = static_cast<std::uint8_t>(value |
                (known[index * 4u + i] ? (1u << i) : 0u));
        return value;
    }

    int run() { return nba97_game_random_seed(&context, &progress); }
};

void verify_complete(std::uint32_t seed, std::uint8_t mask = 0x0f) {
    Fixture f(seed, mask);
    const auto want = expected(seed, mask);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 6 && f.progress.accesses == 6 &&
        f.progress.stores == 6 && f.progress.access_events == 6 &&
        !f.progress.stopped_pc && !f.progress.stopped_address);
    for (unsigned i = 0; i < 6; ++i) {
        check(f.get(i) == want[i].word && f.get_known(i) == want[i].mask);
        check(f.journal[i].pc == StorePc[i] &&
            f.journal[i].address == Base + i * 4u &&
            !(f.journal[i].address & 3u) &&
            f.journal[i].value == want[i].word &&
            f.journal[i].known_mask == want[i].mask &&
            f.journal[i].operation == i + 1u &&
            f.journal[i].width == 4 &&
            f.journal[i].kind == NBA97_GAME_RANDOM_SEED_STORE);
    }
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& actual = f.progress.registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_AT)
            check(actual.word == OperandAt[5] && actual.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_V0)
            check(actual.word == Increment[5] && actual.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_A0)
            check(actual.word == want[5].word && actual.known_mask == want[5].mask);
        else if (i == NBA97_MATCH_INITIALIZE_A1)
            check(actual.word == Base && actual.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_ZERO)
            check(actual.word == 0 && actual.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_RA)
            check(actual.word == 0x800802d8u && actual.known_mask == 0x0f);
        else
            check(actual.word == 0x51000000u + i && actual.known_mask == 0x0f);
    }
}

void seeds_and_exhaustive_natural_domain() {
    verify_complete(0);
    verify_complete(1);
    verify_complete(0xffffu);
    verify_complete(0x80000000u);
    verify_complete(0xffffffffu);
    for (std::uint32_t seed = 0; seed <= 0xffffu; ++seed) {
        Fixture f(seed);
        const auto want = expected(seed, 0x0f);
        check(f.run() == NBA97_TEXT_COMPLETE);
        for (unsigned i = 0; i < 6; ++i)
            check(f.get(i) == want[i].word && f.get_known(i) == 0x0f);
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            want[5].word);
    }
}

void partial_knownness_and_carries() {
    constexpr std::array<std::uint32_t, 5> values{{
        0u, 0xffu, 0x00fffeffu, 0x7fffffffu, 0xffffffffu}};
    for (auto value : values)
        for (std::uint8_t mask = 0; mask < 16; ++mask)
            verify_complete(value, mask);

    Fixture opaque(0xffu, 0x0e);
    opaque.region.known = nullptr;
    check(opaque.run() == NBA97_TEXT_ARGUMENT &&
        opaque.progress.operations == 1 && opaque.progress.accesses == 1 &&
        !opaque.progress.stores &&
        opaque.progress.stopped_pc == StorePc[0] &&
        opaque.progress.stopped_address == Base);

    Fixture known_destination(0xffffffffu, 0x0f);
    known_destination.region.known = nullptr;
    check(known_destination.run() == NBA97_TEXT_COMPLETE &&
        known_destination.progress.stores == 6);
}

void every_store_budget_prefix() {
    constexpr std::uint32_t seed = 0x89abcdefu;
    const auto want = expected(seed, 0x0f);
    for (std::size_t budget = 0; budget < 6; ++budget) {
        Fixture f(seed);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget && f.progress.accesses == budget &&
            f.progress.stores == budget &&
            f.progress.access_events == budget &&
            f.progress.stopped_pc == StorePc[budget] &&
            f.progress.stopped_address == Base +
                static_cast<std::uint32_t>(budget * 4u));
        for (unsigned i = 0; i < 6; ++i)
            check(f.get(i) == (i < budget ? want[i].word : 0xcdcdcdcdu));
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            want[budget].word &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                Increment[budget] &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
                OperandAt[budget] &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == Base);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
            const auto& actual = f.progress.registers.gpr[i];
            if (i == NBA97_MATCH_INITIALIZE_AT)
                check(actual.word == OperandAt[budget] &&
                    actual.known_mask == 0x0f);
            else if (i == NBA97_MATCH_INITIALIZE_V0)
                check(actual.word == Increment[budget] &&
                    actual.known_mask == 0x0f);
            else if (i == NBA97_MATCH_INITIALIZE_A0)
                check(actual.word == want[budget].word &&
                    actual.known_mask == 0x0f);
            else if (i == NBA97_MATCH_INITIALIZE_A1)
                check(actual.word == Base && actual.known_mask == 0x0f);
            else if (i == NBA97_MATCH_INITIALIZE_ZERO)
                check(actual.word == 0 && actual.known_mask == 0x0f);
            else if (i == NBA97_MATCH_INITIALIZE_RA)
                check(actual.word == 0x800802d8u &&
                    actual.known_mask == 0x0f);
            else
                check(actual.word == 0x51000000u + i &&
                    actual.known_mask == 0x0f);
        }
    }
}

void return_mapping_malformed_overlap_and_native_alias() {
    Fixture unknown_ra(0x12345678u);
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stores == 6 && !unknown_ra.progress.completed &&
        unknown_ra.progress.stopped_pc == 0x8009372cu &&
        !unknown_ra.progress.stopped_address);

    Fixture missing;
    missing.context.memory = {nullptr, 0};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.operations == 1 && !missing.progress.stores &&
        missing.progress.stopped_pc == StorePc[0]);

    Fixture short_map;
    short_map.region.size = 20;
    check(short_map.run() == NBA97_TEXT_RESOURCE &&
        short_map.progress.operations == 6 && short_map.progress.stores == 5 &&
        short_map.progress.stopped_pc == StorePc[5]);

    Fixture malformed;
    malformed.known[0] = 2;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1 && !malformed.progress.stores);

    Fixture overlap;
    Nba97GameTextRegion overlap_regions[2] = {
        {Base, overlap.bytes.data(), overlap.known.data(), 8},
        {Base + 4u, overlap.bytes.data() + 4, overlap.known.data() + 4, 8}};
    overlap.context.memory = {overlap_regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT && !overlap.progress.operations);

    std::array<std::uint8_t, 4> alias_bytes{};
    std::array<std::uint8_t, 4> alias_known{{1, 1, 1, 1}};
    std::array<Nba97GameTextRegion, 6> alias_regions{};
    Fixture alias(0x10203040u);
    for (unsigned i = 0; i < alias_regions.size(); ++i)
        alias_regions[i] = {Base + i * 4u, alias_bytes.data(),
            alias_known.data(), alias_bytes.size()};
    alias.context.memory = {alias_regions.data(), alias_regions.size()};
    check(alias.run() == NBA97_TEXT_COMPLETE);
    const auto alias_want = expected(0x10203040u, 0x0f);
    std::uint32_t aliased = 0;
    for (unsigned i = 0; i < 4; ++i)
        aliased |= std::uint32_t(alias_bytes[i]) << (8u * i);
    check(aliased == alias_want[5].word && alias.progress.access_events == 6);

    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT && !bad_zero.progress.operations);
    Fixture bad_mask;
    bad_mask.context.registers.gpr[7].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT && !bad_mask.progress.operations);
    Fixture no_regions;
    no_regions.context.memory = {nullptr, 1};
    check(no_regions.run() == NBA97_TEXT_ARGUMENT);
    Fixture no_journal;
    no_journal.context.access_journal = nullptr;
    check(no_journal.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_region;
    bad_region.region.data = nullptr;
    check(bad_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture wrap_region;
    wrap_region.region.base = 0xfffffff0u;
    wrap_region.region.size = 32;
    check(wrap_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_context;
    check(nba97_game_random_seed(nullptr, &null_context.progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_random_seed(&null_context.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    seeds_and_exhaustive_natural_domain();
    partial_knownness_and_carries();
    every_store_budget_prefix();
    return_mapping_malformed_overlap_and_native_alias();
    std::printf("%u game random seed checks passed\n", checks);
    return 0;
}
