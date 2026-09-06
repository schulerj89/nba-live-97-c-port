#include "recovered/game_rule_delay.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game rule delay check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

struct Fixture {
    Nba97GameRuleDelayContext context{};
    Nba97GameRuleDelayProgress progress{};

    Fixture() {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] = {
                0x10203040u + i * 0x01020304u,
                static_cast<std::uint8_t>(i & 0x0fu)};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x80067dfcu, 0x0f};
        context.machine.hi = {0x89abcdefu, 0x05};
        context.machine.lo = {0x76543210u, 0x0a};
    }

    int run() { return nba97_game_rule_delay(&context, &progress); }
};

bool same_word(Nba97GameRuleDelayWord left,
    Nba97GameRuleDelayWord right) {
    return left.word == right.word && left.known_mask == right.known_mask;
}

void check_machine(const Nba97GameRuleDelayMachine& left,
    const Nba97GameRuleDelayMachine& right) {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(same_word(left.registers.gpr[i], right.registers.gpr[i]));
    check(same_word(left.hi, right.hi));
    check(same_word(left.lo, right.lo));
}

void exact_no_op_and_every_register_knownness() {
    Fixture baseline;
    const auto before = baseline.context.machine;
    check(baseline.run() == NBA97_TEXT_COMPLETE && baseline.progress.completed);
    check(baseline.progress.operations == 0 &&
        baseline.progress.accesses == 0 && baseline.progress.reads == 0 &&
        baseline.progress.stores == 0 && baseline.progress.stopped_pc == 0 &&
        baseline.progress.stopped_address == 0);
    check(same_word(baseline.progress.return_address,
        before.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    check_machine(baseline.progress.machine, before);

    for (unsigned reg = 1; reg < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
            ++reg) {
        if (reg == NBA97_MATCH_INITIALIZE_RA)
            continue;
        for (unsigned mask = 0; mask < 16; ++mask) {
            Fixture f;
            f.context.machine.registers.gpr[reg] = {
                0xfedcba98u ^ (reg * 0x01010101u),
                static_cast<std::uint8_t>(mask)};
            const auto input = f.context.machine;
            check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
            check_machine(f.progress.machine, input);
            check(f.progress.operations == 0 && f.progress.accesses == 0 &&
                f.progress.reads == 0 && f.progress.stores == 0);
        }
    }

    for (unsigned mask = 0; mask < 16; ++mask) {
        Fixture hi;
        hi.context.machine.hi = {0xa5a50000u | mask,
            static_cast<std::uint8_t>(mask)};
        const auto hi_before = hi.context.machine;
        check(hi.run() == NBA97_TEXT_COMPLETE);
        check_machine(hi.progress.machine, hi_before);

        Fixture lo;
        lo.context.machine.lo = {0x5a5a0000u | mask,
            static_cast<std::uint8_t>(mask)};
        const auto lo_before = lo.context.machine;
        check(lo.run() == NBA97_TEXT_COMPLETE);
        check_machine(lo.progress.machine, lo_before);
    }
}

void live_ra_masks_and_arbitrary_targets() {
    constexpr std::array<std::uint32_t, 6> targets{{
        0u, 1u, 3u, 0x7fffffffu, 0x80000001u, 0xffffffffu}};
    for (const auto target : targets) {
        for (unsigned mask = 0; mask < 16; ++mask) {
            Fixture f;
            f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
                target, static_cast<std::uint8_t>(mask)};
            const auto before = f.context.machine;
            const int result = f.run();
            check(result == (mask == 15 ? NBA97_TEXT_COMPLETE :
                NBA97_TEXT_UNKNOWN));
            check(bool(f.progress.completed) == (mask == 15));
            check(f.progress.operations == 0 && f.progress.accesses == 0 &&
                f.progress.reads == 0 && f.progress.stores == 0);
            check(same_word(f.progress.return_address,
                before.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
            check_machine(f.progress.machine, before);
            if (mask == 15) {
                check(f.progress.stopped_pc == 0 &&
                    f.progress.stopped_address == 0);
            } else {
                check(f.progress.stopped_pc == 0x800295c8u &&
                    f.progress.stopped_address == target);
            }
        }
    }
}

void ignored_a0_sp_and_repeatability() {
    Fixture f;
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {20000u, 0};
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0xffffffffu, 0};
    const auto before = f.context.machine;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check_machine(f.progress.machine, before);

    for (unsigned i = 0; i < 10000; ++i) {
        Fixture repeated;
        repeated.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {i & 1u ? 5000u : 20000u, 0x0f};
        check(repeated.run() == NBA97_TEXT_COMPLETE &&
            repeated.progress.operations == 0 &&
            repeated.progress.accesses == 0 &&
            repeated.progress.reads == 0 && repeated.progress.stores == 0);
    }
}

void invalid_arguments_and_machine_metadata() {
    Nba97GameRuleDelayProgress progress{};
    Fixture valid;
    check(nba97_game_rule_delay(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_rule_delay(&valid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

    Fixture bad_zero_word;
    bad_zero_word.context.machine.registers.gpr[0].word = 1;
    check(bad_zero_word.run() == NBA97_TEXT_ARGUMENT &&
        !bad_zero_word.progress.completed);
    Fixture bad_zero_mask;
    bad_zero_mask.context.machine.registers.gpr[0].known_mask = 0;
    check(bad_zero_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_gpr_mask;
    bad_gpr_mask.context.machine.registers.gpr[7].known_mask = 0x10;
    check(bad_gpr_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_ra_mask;
    bad_ra_mask.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 0xff;
    check(bad_ra_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_hi_mask;
    bad_hi_mask.context.machine.hi.known_mask = 0x10;
    check(bad_hi_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_lo_mask;
    bad_lo_mask.context.machine.lo.known_mask = 0xff;
    check(bad_lo_mask.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_no_op_and_every_register_knownness();
    live_ra_masks_and_arbitrary_targets();
    ignored_a0_sp_and_repeatability();
    invalid_arguments_and_machine_metadata();
    std::printf("%u game rule delay checks passed\n", checks);
    return 0;
}
