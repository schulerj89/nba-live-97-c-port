#include "recovered/game_late_period_limits.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game late-period-limits check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Clock = 0x800fdb58u;
constexpr std::uint32_t Period = 0x800fdb68u;
constexpr std::uint32_t Limit = 0x8010606cu;
constexpr std::uint32_t Home = 0x8001ee24u;
constexpr std::uint32_t Away = 0x8001eee8u;

struct Cell {
    std::array<std::uint8_t, 4> data{};
    std::array<std::uint8_t, 4> known{{1, 1, 1, 1}};
};

struct Fixture {
    Cell clock;
    Cell period;
    Cell limit;
    Cell home;
    Cell away;
    std::array<Nba97GameTextRegion, 5> regions{{
        {Clock, clock.data.data(), clock.known.data(), 4},
        {Period, period.data.data(), period.known.data(), 2},
        {Limit, limit.data.data(), limit.known.data(), 2},
        {Home, home.data.data(), home.known.data(), 2},
        {Away, away.data.data(), away.known.data(), 2}
    }};
    std::array<Nba97GameLatePeriodLimitsAccess, 16> journal{};
    Nba97GameLatePeriodLimitsContext context{};
    Nba97GameLatePeriodLimitsProgress progress{};

    Fixture(std::uint32_t clock_value = 0, std::uint16_t period_value = 3,
        std::uint16_t home_value = 0, std::uint16_t away_value = 0) {
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = journal.size();
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x41000000u + i * 0x00010101u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x80068cf4u, 0x0f};
        put(clock, clock_value, 4);
        put(period, period_value, 2);
        put(limit, 0xa55au, 2);
        put(home, home_value, 2);
        put(away, away_value, 2);
    }

    static void put(Cell& cell, std::uint32_t value, unsigned width,
        std::uint8_t mask = 0x0f) {
        for (unsigned i = 0; i < width; ++i) {
            cell.data[i] = static_cast<std::uint8_t>(value >> (i * 8u));
            cell.known[i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }

    static std::uint32_t get(const Cell& cell, unsigned width) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(cell.data[i]) << (i * 8u);
        return value;
    }

    int run() { return nba97_game_late_period_limits(&context, &progress); }
};

void check_untouched(const Fixture& f, unsigned except0, unsigned except1,
    unsigned except2, unsigned except3, unsigned except4) {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == except0 || i == except1 || i == except2 || i == except3 ||
            i == except4)
            continue;
        const std::uint32_t expected = i == NBA97_MATCH_INITIALIZE_ZERO ? 0u :
            (i == NBA97_MATCH_INITIALIZE_RA ? 0x80068cf4u :
                0x41000000u + i * 0x00010101u);
        check(f.progress.registers.gpr[i].word == expected &&
            f.progress.registers.gpr[i].known_mask == 0x0f);
    }
}

void clock_boundaries_and_delay_store() {
    struct Case { std::uint32_t clock; bool eligible; };
    constexpr Case cases[] = {
        {0xffffffffu, true}, {0x00001c1fu, true},
        {0x00001c20u, false}, {0x7fffffffu, false},
        {0x80000000u, true}
    };
    for (const auto& c : cases) {
        Fixture f(c.clock);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(Fixture::get(f.limit, 2) == (c.eligible ? 5u : 0u));
        if (c.eligible) {
            check(Fixture::get(f.home, 2) == 3u &&
                Fixture::get(f.away, 2) == 3u &&
                f.progress.operations == 9);
        } else {
            check(Fixture::get(f.home, 2) == 0u &&
                Fixture::get(f.away, 2) == 0u &&
                f.progress.operations == 2);
            check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                    0u &&
                f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                    Limit);
            check_untouched(f, NBA97_MATCH_INITIALIZE_V0,
                NBA97_MATCH_INITIALIZE_A0, 99, 99, 99);
        }
    }

    Fixture unknown(0);
    Fixture::put(unknown.clock, 0, 4, 0x07);
    check(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.progress.operations == 2 && unknown.progress.stores == 1 &&
        unknown.progress.stopped_pc == 0x80067564u &&
        Fixture::get(unknown.limit, 2) == 0u);
    check(unknown.journal[1].pc == 0x80067568u &&
        unknown.journal[1].kind == NBA97_GAME_LATE_PERIOD_LIMITS_STORE &&
        unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e);

    Fixture known_negative(0xdeadbeefu);
    Fixture::put(known_negative.clock, 0x80ffffffu, 4, 0x08);
    check(known_negative.run() == NBA97_TEXT_COMPLETE &&
        known_negative.progress.selected_late_period_limit);
}

void signed_period_boundaries_and_gprs() {
    struct Case { std::uint16_t period; std::uint16_t limit; bool eligible; };
    constexpr Case cases[] = {
        {0xffffu, 0u, false}, {0u, 0u, false}, {2u, 0u, false},
        {3u, 5u, true}, {4u, 4u, true}, {0x7fffu, 4u, true}
    };
    for (const auto& c : cases) {
        Fixture f(0, c.period);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(Fixture::get(f.limit, 2) == c.limit);
        const auto& r = f.progress.registers.gpr;
        if (!c.eligible) {
            check(r[NBA97_MATCH_INITIALIZE_V1].word ==
                (c.period & 0x8000u ? 0xffff0000u | c.period : c.period));
            check(r[NBA97_MATCH_INITIALIZE_V0].word == 1u &&
                r[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f &&
                r[NBA97_MATCH_INITIALIZE_A0].word == Limit &&
                f.progress.operations == 3);
            check_untouched(f, NBA97_MATCH_INITIALIZE_V0,
                NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_A0, 99, 99);
        } else {
            const std::uint32_t target = c.limit - 2u;
            check(r[NBA97_MATCH_INITIALIZE_AT].word == 0x80020000u &&
                r[NBA97_MATCH_INITIALIZE_V0].word == target &&
                r[NBA97_MATCH_INITIALIZE_V1].word == c.limit &&
                r[NBA97_MATCH_INITIALIZE_A0].word == target &&
                r[NBA97_MATCH_INITIALIZE_A1].word == Home);
            check_untouched(f, NBA97_MATCH_INITIALIZE_AT,
                NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V1,
                NBA97_MATCH_INITIALIZE_A0, NBA97_MATCH_INITIALIZE_A1);
        }
    }

    Fixture unknown_first(0, 0);
    Fixture::put(unknown_first.period, 0, 2, 0x01);
    check(unknown_first.run() == NBA97_TEXT_UNKNOWN &&
        unknown_first.progress.stopped_pc == 0x8006757cu &&
        unknown_first.progress.operations == 3 &&
        unknown_first.progress.registers.gpr
            [NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e);

}

void home_away_limits_and_precise_branches() {
    constexpr std::uint16_t values[] = {0u, 3u, 2u, 4u, 0xffffu};
    for (auto home : values) {
        for (auto away : values) {
            Fixture f(0, 3, home, away);
            check(f.run() == NBA97_TEXT_COMPLETE);
            check(Fixture::get(f.home, 2) == (home < 3u ? 3u : home));
            check(Fixture::get(f.away, 2) == (away < 3u ? 3u : away));
            check(f.progress.home_raised == (home < 3u) &&
                f.progress.away_raised == (away < 3u));
            check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                    3u &&
                f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
                    5u &&
                f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                    3u &&
                f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                    Home);
            const std::uint32_t expected_at = away < 3u ? 0x80020000u :
                0x41000000u + NBA97_MATCH_INITIALIZE_AT * 0x00010101u;
            check(f.progress.registers.gpr
                [NBA97_MATCH_INITIALIZE_AT].word == expected_at);
        }
    }

    Fixture equal4(0, 4, 2, 2);
    check(equal4.run() == NBA97_TEXT_COMPLETE &&
        Fixture::get(equal4.home, 2) == 2u &&
        Fixture::get(equal4.away, 2) == 2u);

    Fixture unknown_home(0, 3, 0, 0);
    Fixture::put(unknown_home.home, 0, 2, 0);
    check(unknown_home.run() == NBA97_TEXT_UNKNOWN &&
        unknown_home.progress.operations == 6 &&
        unknown_home.progress.stopped_pc == 0x800675b0u &&
        unknown_home.progress.registers.gpr
            [NBA97_MATCH_INITIALIZE_V0].word == 3u &&
        Fixture::get(unknown_home.home, 2) == 0u);

    Fixture provably_above(0, 3, 0x0100u, 0);
    Fixture::put(provably_above.home, 0x0100u, 2, 0x02);
    check(provably_above.run() == NBA97_TEXT_COMPLETE &&
        !provably_above.progress.home_raised &&
        Fixture::get(provably_above.away, 2) == 3u);

    Fixture unknown_away(0, 3, 4, 0);
    Fixture::put(unknown_away.away, 0, 2, 0);
    check(unknown_away.run() == NBA97_TEXT_UNKNOWN &&
        unknown_away.progress.operations == 7 &&
        unknown_away.progress.stopped_pc == 0x800675ccu &&
        unknown_away.progress.registers.gpr
            [NBA97_MATCH_INITIALIZE_V0].word == 3u);
}

void exact_access_order_alias_and_alignment() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE);
    constexpr std::uint32_t pcs[] = {
        0x80067554u, 0x80067568u, 0x80067570u, 0x80067590u,
        0x800675a0u, 0x800675a4u, 0x800675b8u, 0x800675c0u,
        0x800675d8u
    };
    constexpr std::uint32_t addresses[] = {
        Clock, Limit, Period, Limit, Limit, Home, Home, Away, Away
    };
    constexpr std::uint8_t kinds[] = {
        NBA97_GAME_LATE_PERIOD_LIMITS_READ,
        NBA97_GAME_LATE_PERIOD_LIMITS_STORE,
        NBA97_GAME_LATE_PERIOD_LIMITS_READ,
        NBA97_GAME_LATE_PERIOD_LIMITS_STORE,
        NBA97_GAME_LATE_PERIOD_LIMITS_READ,
        NBA97_GAME_LATE_PERIOD_LIMITS_READ,
        NBA97_GAME_LATE_PERIOD_LIMITS_STORE,
        NBA97_GAME_LATE_PERIOD_LIMITS_READ,
        NBA97_GAME_LATE_PERIOD_LIMITS_STORE
    };
    check(f.progress.access_events == std::size(pcs));
    for (std::size_t i = 0; i < std::size(pcs); ++i) {
        check(f.journal[i].pc == pcs[i] &&
            f.journal[i].address == addresses[i] &&
            f.journal[i].operation == i + 1u &&
            f.journal[i].kind == kinds[i] &&
            f.journal[i].width == (i == 0 ? 4u : 2u) &&
            !(f.journal[i].address & (f.journal[i].width - 1u)));
    }

    Fixture home_away_alias;
    home_away_alias.regions[4].data = home_away_alias.home.data.data();
    home_away_alias.regions[4].known = home_away_alias.home.known.data();
    check(home_away_alias.run() == NBA97_TEXT_COMPLETE &&
        home_away_alias.progress.home_raised &&
        !home_away_alias.progress.away_raised &&
        home_away_alias.progress.away_before.word == 3u &&
        home_away_alias.progress.operations == 8);

    Fixture limit_period_alias;
    limit_period_alias.regions[2].data = limit_period_alias.period.data.data();
    limit_period_alias.regions[2].known = limit_period_alias.period.known.data();
    check(limit_period_alias.run() == NBA97_TEXT_COMPLETE &&
        limit_period_alias.progress.period.word == 0u &&
        limit_period_alias.progress.operations == 3);
}

void every_operation_budget_prefix() {
    Fixture baseline;
    check(baseline.run() == NBA97_TEXT_COMPLETE &&
        baseline.progress.operations == 9);
    constexpr std::uint32_t stopped[] = {
        0x80067554u, 0x80067568u, 0x80067570u, 0x80067590u,
        0x800675a0u, 0x800675a4u, 0x800675b8u, 0x800675c0u,
        0x800675d8u
    };
    constexpr std::uint32_t at[] = {
        0x41010101u, 0x41010101u, 0x41010101u,
        0x41010101u, 0x41010101u, 0x41010101u,
        0x41010101u, 0x41010101u, 0x80020000u
    };
    constexpr std::uint32_t v0[] = {
        0x80100000u, 1u, 1u, 5u, 5u, 5u, 3u, 0x80020000u, 3u
    };
    constexpr std::uint32_t v1[] = {
        0x41030303u, 0x41030303u, 0x80100000u, 3u,
        0x80100000u, 5u, 5u, 5u, 5u
    };
    constexpr std::uint32_t a0[] = {
        0x41040404u, Limit, Limit, Limit, Limit, Limit, 3u, 3u, 3u
    };
    constexpr std::uint32_t a1[] = {
        0x41050505u, 0x41050505u, 0x41050505u, 0x41050505u,
        Home, Home, Home, Home, Home
    };
    for (std::size_t budget = 0; budget < std::size(stopped); ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.access_events == budget &&
            f.progress.stopped_pc == stopped[budget]);
        for (std::size_t i = 0; i < budget; ++i)
            check(f.journal[i].pc == baseline.journal[i].pc &&
                f.journal[i].address == baseline.journal[i].address &&
                f.journal[i].value == baseline.journal[i].value &&
                f.journal[i].known_mask == baseline.journal[i].known_mask &&
                f.journal[i].kind == baseline.journal[i].kind);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
            std::uint32_t expected = i == NBA97_MATCH_INITIALIZE_ZERO ? 0u :
                (i == NBA97_MATCH_INITIALIZE_RA ? 0x80068cf4u :
                    0x41000000u + i * 0x00010101u);
            if (i == NBA97_MATCH_INITIALIZE_AT) expected = at[budget];
            if (i == NBA97_MATCH_INITIALIZE_V0) expected = v0[budget];
            if (i == NBA97_MATCH_INITIALIZE_V1) expected = v1[budget];
            if (i == NBA97_MATCH_INITIALIZE_A0) expected = a0[budget];
            if (i == NBA97_MATCH_INITIALIZE_A1) expected = a1[budget];
            check(f.progress.registers.gpr[i].word == expected &&
                f.progress.registers.gpr[i].known_mask == 0x0f);
        }
    }
}

void all_control_flow_budget_prefixes() {
    struct Case {
        std::uint32_t clock;
        std::uint16_t period;
        std::uint16_t home;
        std::uint16_t away;
    };
    constexpr Case cases[] = {
        {0x1c20u, 3u, 0u, 0u},
        {0u, 2u, 0u, 0u},
        {0u, 3u, 4u, 4u},
        {0u, 3u, 4u, 0u},
        {0u, 3u, 0u, 4u},
        {0u, 3u, 0u, 0u}
    };
    for (const auto& c : cases) {
        Fixture baseline(c.clock, c.period, c.home, c.away);
        check(baseline.run() == NBA97_TEXT_COMPLETE);
        for (std::size_t budget = 0; budget < baseline.progress.operations;
             ++budget) {
            Fixture f(c.clock, c.period, c.home, c.away);
            f.context.operation_budget = budget;
            check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
                f.progress.operations == budget &&
                f.progress.access_events == budget &&
                f.progress.stopped_pc == baseline.journal[budget].pc &&
                f.progress.stopped_address ==
                    baseline.journal[budget].address);
            for (std::size_t i = 0; i < budget; ++i)
                check(f.journal[i].pc == baseline.journal[i].pc &&
                    f.journal[i].address == baseline.journal[i].address &&
                    f.journal[i].value == baseline.journal[i].value &&
                    f.journal[i].known_mask ==
                        baseline.journal[i].known_mask &&
                    f.journal[i].kind == baseline.journal[i].kind);
        }
    }
}

void failure_mapping_validation_unknown_return() {
    Fixture missing;
    missing.context.memory = {nullptr, 0};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.operations == 1 &&
        missing.progress.stopped_pc == 0x80067554u);

    Fixture missing_store;
    missing_store.context.memory.count = 2;
    check(missing_store.run() == NBA97_TEXT_RESOURCE &&
        missing_store.progress.operations == 2 &&
        missing_store.progress.reads == 1 && !missing_store.progress.stores &&
        missing_store.progress.stopped_pc == 0x80067568u);

    Fixture malformed;
    malformed.clock.known[0] = 2;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1 && !malformed.progress.reads);

    Fixture unknown_ra;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 9 && unknown_ra.progress.stores == 4 &&
        unknown_ra.progress.stopped_pc == 0x800675dcu &&
        Fixture::get(unknown_ra.limit, 2) == 5u &&
        Fixture::get(unknown_ra.home, 2) == 3u &&
        Fixture::get(unknown_ra.away, 2) == 3u);

    Fixture overlap;
    auto overlapping = overlap.regions;
    overlapping[1].base = Clock + 2u;
    overlap.context.memory = {overlapping.data(), overlapping.size()};
    check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        !overlap.progress.operations);

    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_mask;
    bad_mask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_regions;
    null_regions.context.memory = {nullptr, 1};
    check(null_regions.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_journal;
    null_journal.context.access_journal = nullptr;
    check(null_journal.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_region;
    bad_region.regions[0].data = nullptr;
    check(bad_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture wrapping_region;
    wrapping_region.regions[0].base = 0xfffffff0u;
    wrapping_region.regions[0].size = 32;
    check(wrapping_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_context;
    check(nba97_game_late_period_limits(nullptr, &null_context.progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_late_period_limits(&null_context.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    clock_boundaries_and_delay_store();
    signed_period_boundaries_and_gprs();
    home_away_limits_and_precise_branches();
    exact_access_order_alias_and_alignment();
    every_operation_budget_prefix();
    all_control_flow_budget_prefixes();
    failure_mapping_validation_unknown_return();
    std::printf("%u game late-period-limits checks passed\n", checks);
    return 0;
}
