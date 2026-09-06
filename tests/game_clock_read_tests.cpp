#include "recovered/game_clock_read.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game clock read check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Clock = 0x800d7a70u;

struct Fixture {
    std::array<std::uint8_t, 12> bytes{};
    std::array<std::uint8_t, 12> known{};
    std::array<Nba97GameTextRegion, 1> regions{{
        {Clock - 4u, bytes.data(), known.data(), bytes.size()}}};
    std::array<Nba97GameClockReadAccess, 2> journal{};
    Nba97GameClockReadContext context{};
    Nba97GameClockReadProgress progress{};

    Fixture() {
        known.fill(1);
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 1;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] = {
                0x10203040u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
            0x807fff03u, 0};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002a278u, 0x0f};
        context.machine.hi = {0x89abcdefu, 0x05};
        context.machine.lo = {0x76543210u, 0x0a};
        put(0, 0x12345678u, 0x0f);
    }

    void put(unsigned offset, std::uint32_t value, std::uint8_t mask) {
        for (unsigned i = 0; i < 4; ++i) {
            bytes[4u + offset + i] =
                static_cast<std::uint8_t>(value >> (i * 8u));
            known[4u + offset + i] = static_cast<std::uint8_t>(mask >> i) & 1u;
        }
    }

    int run() { return nba97_game_clock_read(&context, &progress); }
};

bool same_word(Nba97GameClockReadWord a, Nba97GameClockReadWord b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}

void counter_extremes_masks_and_machine_preservation() {
    constexpr std::array<std::uint32_t, 5> values{{
        0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}};
    for (const auto value : values) {
        for (unsigned mask = 0; mask < 16; ++mask) {
            Fixture f;
            f.put(0, value, static_cast<std::uint8_t>(mask));
            const auto before = f.context.machine;
            const auto memory_before = f.bytes;
            check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
            check(f.progress.operations == 1 && f.progress.accesses == 1 &&
                f.progress.reads == 1 && f.progress.stores == 0 &&
                f.progress.access_events == 1);
            check(f.progress.return_v0.word == value &&
                f.progress.return_v0.known_mask == mask &&
                same_word(f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V0], f.progress.return_v0));
            for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
                if (i != NBA97_MATCH_INITIALIZE_V0)
                    check(same_word(f.progress.machine.registers.gpr[i],
                        before.registers.gpr[i]));
            check(same_word(f.progress.machine.hi, before.hi) &&
                same_word(f.progress.machine.lo, before.lo));
            check(f.bytes == memory_before);
            check(f.journal[0].pc == 0x800a5814u &&
                f.journal[0].address == Clock && f.journal[0].value == value &&
                f.journal[0].operation == 1 && f.journal[0].width == 4 &&
                f.journal[0].known_mask == mask &&
                f.journal[0].kind == NBA97_GAME_CLOCK_READ_READ);
            check(f.progress.stopped_pc == 0 &&
                f.progress.stopped_address == 0);
        }
    }
}

void budget_mapping_metadata_and_lui_prefix() {
    Fixture budget;
    const auto untouched = budget.context.machine.registers.gpr[3];
    budget.context.operation_budget = 0;
    check(budget.run() == NBA97_TEXT_LIMIT && !budget.progress.completed &&
        budget.progress.operations == 0 && budget.progress.accesses == 0 &&
        budget.progress.reads == 0 && budget.progress.stores == 0 &&
        budget.progress.access_events == 0 &&
        budget.progress.stopped_pc == 0x800a5814u &&
        budget.progress.stopped_address == Clock);
    check(budget.progress.return_v0.word == 0x800d0000u &&
        budget.progress.return_v0.known_mask == 0x0f &&
        same_word(budget.progress.machine.registers.gpr[3], untouched));

    Fixture unmapped;
    unmapped.context.memory = {nullptr, 0};
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.operations == 1 && unmapped.progress.accesses == 1 &&
        unmapped.progress.reads == 0 && unmapped.progress.access_events == 0 &&
        unmapped.progress.return_v0.word == 0x800d0000u &&
        unmapped.progress.stopped_pc == 0x800a5814u);

    Fixture truncated;
    truncated.regions[0].size = 7;
    check(truncated.run() == NBA97_TEXT_RESOURCE &&
        truncated.progress.return_v0.word == 0x800d0000u);

    Fixture malformed;
    malformed.known[6] = 2;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1 && malformed.progress.accesses == 1 &&
        malformed.progress.reads == 0 &&
        malformed.progress.return_v0.word == 0x800d0000u);

    Fixture all_known;
    all_known.regions[0].known = nullptr;
    check(all_known.run() == NBA97_TEXT_COMPLETE &&
        all_known.progress.return_v0.word == 0x12345678u &&
        all_known.progress.return_v0.known_mask == 0x0f);
}

void unknown_ra_is_consumed_after_the_read() {
    Fixture f;
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x8002a278u, 0x07};
    f.put(0, 0xcafebabeu, 0x05);
    check(f.run() == NBA97_TEXT_UNKNOWN && !f.progress.completed &&
        f.progress.operations == 1 && f.progress.reads == 1 &&
        f.progress.access_events == 1 &&
        f.progress.return_v0.word == 0xcafebabeu &&
        f.progress.return_v0.known_mask == 0x05 &&
        f.progress.stopped_pc == 0x800a5818u &&
        f.progress.stopped_address == 0x8002a278u);
}

void invalid_arguments_regions_and_machine_metadata() {
    Nba97GameClockReadProgress progress{};
    Fixture f;
    check(nba97_game_clock_read(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_clock_read(&f.context, nullptr) == NBA97_TEXT_ARGUMENT);

    Fixture missing_regions;
    missing_regions.context.memory.region = nullptr;
    check(missing_regions.run() == NBA97_TEXT_ARGUMENT);

    Fixture missing_journal;
    missing_journal.context.access_journal = nullptr;
    check(missing_journal.run() == NBA97_TEXT_ARGUMENT);

    Fixture null_data;
    null_data.regions[0].data = nullptr;
    check(null_data.run() == NBA97_TEXT_ARGUMENT);

    Fixture zero_size;
    zero_size.regions[0].size = 0;
    check(zero_size.run() == NBA97_TEXT_ARGUMENT);

    Fixture overflow;
    overflow.regions[0].base = 0xfffffffcu;
    overflow.regions[0].size = 8;
    check(overflow.run() == NBA97_TEXT_ARGUMENT);

    Fixture overlap;
    std::array<Nba97GameTextRegion, 2> regions{{
        overlap.regions[0], overlap.regions[0]}};
    overlap.context.memory = {regions.data(), regions.size()};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);

    Fixture bad_zero;
    bad_zero.context.machine.registers.gpr[0] = {1, 0x0f};
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_gpr_mask;
    bad_gpr_mask.context.machine.registers.gpr[7].known_mask = 0x10;
    check(bad_gpr_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_hi_mask;
    bad_hi_mask.context.machine.hi.known_mask = 0x10;
    check(bad_hi_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_lo_mask;
    bad_lo_mask.context.machine.lo.known_mask = 0xff;
    check(bad_lo_mask.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    counter_extremes_masks_and_machine_preservation();
    budget_mapping_metadata_and_lui_prefix();
    unknown_ra_is_consumed_after_the_read();
    invalid_arguments_regions_and_machine_metadata();
    std::printf("%u game clock read checks passed\n", checks);
    return 0;
}
