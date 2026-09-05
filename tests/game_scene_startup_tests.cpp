#include "game_scene_startup.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t Base = 0x80000000u;
constexpr std::size_t Size = 0x120000u;
constexpr std::uint32_t Stack = 0x8010ff00u;
constexpr std::uint32_t Selector = 0x8001ede8u;
constexpr std::uint32_t EntityTableRoot = 0x800fc650u;
constexpr std::uint32_t EntityOutput = 0x800fee90u;

std::size_t checks;
void check_at(bool condition, int line) {
    ++checks;
    if (!condition) throw std::runtime_error(
        "scene startup check failed at line " + std::to_string(line));
}
#define check(condition) check_at((condition), __LINE__)

struct SeenCall {
    Nba97GameSceneStartupEvent event{};
    Nba97GameSceneStartupRegisters registers{};
};

struct Fixture {
    enum Mode {
        Normal,
        ControllerMutation,
        ControllerNegative,
        LastControllerUnknownBase,
        ControllerUnknownCounter,
        RosterMinusOne,
        RosterRunaway,
        RosterUnknownCounter,
        SelectorMutation,
        UnknownDisplayDelayBase,
        RelocateStack,
        UnknownFinalStack,
        UnknownReturnAddress,
        Refuse,
        Malformed
    } mode = Normal;
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
    std::vector<Nba97GameTextRegion> regions;
    std::vector<Nba97GameSceneStartupAccess> journal =
        std::vector<Nba97GameSceneStartupAccess>(256);
    std::vector<SeenCall> calls;
    Nba97GameSceneStartupContext context{};
    Nba97GameSceneStartupProgress progress{};
    std::size_t refuse_at = static_cast<std::size_t>(-1);

    Fixture() { reset(); }

    std::size_t offset(std::uint32_t address) const {
        check(address >= Base && address - Base < bytes.size());
        return address - Base;
    }
    void put(std::uint32_t address, std::uint32_t word, unsigned width = 4) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(word >> (8u * i));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
        auto at = offset(address);
        std::uint32_t word = 0;
        for (unsigned i = 0; i < width; ++i)
            word |= static_cast<std::uint32_t>(bytes[at + i]) << (8u * i);
        return word;
    }
    void seed() {
        for (unsigned i = 0; i < 12; ++i) {
            auto home = 0x80030000u + i * 4u;
            auto away = 0x80030100u + i * 4u;
            put(0x80020b8cu + i * 4u, home);
            put(0x80020bbcu + i * 4u, away);
            put(home, static_cast<std::uint16_t>(-300 + static_cast<int>(i)), 2);
            put(away, static_cast<std::uint16_t>(200 + i), 2);
        }
        put(0x80020becu, 0x80030130u);
        put(0x80030130u, static_cast<std::uint16_t>(212), 2);
        put(EntityTableRoot, 0x80040000u);
        for (unsigned i = 0; i < 10; ++i) {
            auto entity = 0x80041000u + i * 0x40u;
            auto roster = 0x80042000u + i * 4u;
            put(0x80040000u + i * 4u, entity);
            put(entity + 0x20u, roster);
            put(roster, static_cast<std::uint16_t>(i & 1u ? 1000 + i : -1000 - static_cast<int>(i)), 2);
        }
        put(0x800b729cu, 0x800abc00u);
        put(Selector, 0);
        put(0x800fa636u, 0x55aau, 2);
    }
    void reset() {
        mode = Normal;
        refuse_at = static_cast<std::size_t>(-1);
        std::fill(bytes.begin(), bytes.end(), std::uint8_t{0xcd});
        std::fill(known.begin(), known.end(), std::uint8_t{1});
        calls.clear();
        regions.clear();
        regions.push_back({Base, bytes.data(), known.data(), bytes.size()});
        context = {};
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 10000;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u, 0x0f};
        seed();
    }
    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameSceneStartupEvent* event,
        Nba97GameSceneStartupRegisters* registers) {
        auto& fixture = *static_cast<Fixture*>(opaque);
        const auto index = fixture.calls.size();
        fixture.calls.push_back({*event, *registers});
        if (fixture.mode == Refuse && index == fixture.refuse_at)
            return 0;
        if (fixture.mode == Malformed && index == fixture.refuse_at) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 0x10;
            return 1;
        }
        if (event->kind == NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224) {
            if (fixture.mode == ControllerMutation && index == 0) {
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {7, 0x0f};
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {0x800fabd0u, 0x0f};
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = {9, 0x0f};
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 3] = {0x7777u, 0x0f};
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x7777u, 0x0f};
            } else if (fixture.mode == ControllerNegative && index == 0) {
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0xffffffffu, 0x0f};
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
            } else if (fixture.mode == LastControllerUnknownBase && index == 7) {
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {0x800fab20u, 0x0e};
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
            } else if (fixture.mode == ControllerUnknownCounter && index == 0) {
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {7, 0x0e};
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
            } else {
                const auto slot = registers->gpr[NBA97_MATCH_INITIALIZE_A0].word;
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                    (slot & 1u) ? 0u : 0x3e1au, 0x0f};
            }
        } else if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_8004D38C) {
            if (fixture.mode == RosterMinusOne)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0xffffffffu, 0x0f};
            if (fixture.mode == RosterRunaway)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0x7fffffffu, 0x0f};
            if (fixture.mode == RosterUnknownCounter)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0x12345678u, 0x0e};
        } else if (fixture.mode == SelectorMutation &&
            event->pc == 0x80048f20u) {
            fixture.put(Selector, 7);
            registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {0x80022000u, 0x0f};
        } else if (fixture.mode == SelectorMutation &&
            event->pc == 0x80048f4cu) {
            fixture.put(Selector, 0);
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0x80021000u, 0x0f};
        } else if (fixture.mode == UnknownDisplayDelayBase &&
            event->pc == 0x80048f4cu) {
            registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {0x80022000u, 0x0e};
        } else if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_80056944) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 0x0f};
            if (fixture.mode == UnknownFinalStack)
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {0x8010fd00u, 0x0e};
            if (fixture.mode == RelocateStack || fixture.mode == UnknownReturnAddress) {
                const std::uint32_t alternate = 0x8010fd00u;
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {alternate, 0x0f};
                fixture.put(alternate + 0x20u, 0x8fedcba0u);
                fixture.put(alternate + 0x1cu, 0x33333333u);
                fixture.put(alternate + 0x18u, 0x22222222u);
                fixture.put(alternate + 0x14u, 0x11111111u);
                fixture.put(alternate + 0x10u, 0x00000000u);
                if (fixture.mode == UnknownReturnAddress)
                    fixture.known[fixture.offset(alternate + 0x20u)] = 0;
            }
        }
        return 1;
    }
    int run() {
        context.memory = {regions.data(), regions.size()};
        return nba97_game_scene_startup(&context, &progress);
    }
};

void normal_path_and_exact_order() {
    Fixture fixture;
    const auto initial = fixture.context.registers;
    check(fixture.run() == NBA97_TEXT_COMPLETE);
    check(fixture.progress.completed && fixture.progress.operations == 184);
    check(fixture.progress.accesses == 165 && fixture.progress.reads == 98);
    check(fixture.progress.stores == 67 && fixture.progress.callbacks_completed == 19);
    check(fixture.progress.controller_iterations == 8 && fixture.progress.controller_matches == 4);
    check(fixture.progress.roster_iterations == 12 && fixture.progress.entity_iterations == 10);
    check(fixture.calls.size() == 19);
    for (unsigned i = 0; i < 8; ++i) {
        check(fixture.calls[i].event.pc == 0x80048dacu);
        check(fixture.calls[i].event.delay_slot_pc == 0x80048db0u);
        check(fixture.calls[i].event.entry == 0x8008f224u);
        check(fixture.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == i);
        check(fixture.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x80048db4u);
        check(fixture.get(0x800faba4u + i * 4u) == ((i & 1u) ? 0u : 2u));
    }
    const std::uint32_t pcs[] = {0x80048df0u,0x80048e94u,0x80048e9cu,
        0x80048eacu,0x80048eb8u,0x80048f20u,0x80048f4cu,0x80048f78u,
        0x80048fa0u,0x80048fb4u,0x80048fbcu};
    const std::uint32_t entries[] = {0x8004d38cu,0x80052c20u,0x800a7738u,
        0x80056074u,0x8005605cu,0x80099ca4u,0x80099accu,0x80099ca4u,
        0x80099accu,0x80063edcu,0x80056944u};
    for (unsigned i = 0; i < 11; ++i) {
        check(fixture.calls[8 + i].event.pc == pcs[i]);
        check(fixture.calls[8 + i].event.delay_slot_pc == pcs[i] + 4u);
        check(fixture.calls[8 + i].event.entry == entries[i]);
        check(fixture.calls[8 + i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == pcs[i] + 8u);
    }
    check(fixture.calls[8].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 0);
    check(fixture.calls[11].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x800abc00u);
    check(fixture.calls[12].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x100u);
    check(fixture.calls[12].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x78u);
    check(fixture.calls[13].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x80022070u);
    check(fixture.calls[14].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x80021f48u);
    check(fixture.calls[15].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x8002205cu);
    check(fixture.calls[16].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x80021eecu);
    check(fixture.get(0x800eb684u) == 0 && fixture.get(0x800fa374u) == 0);
    check(fixture.get(0x80104248u) == 0 && fixture.get(0x800febf4u) == 0);
    check(fixture.get(0x800eb678u) == 0 && fixture.get(0x800fa62cu) == 0);
    check(fixture.get(0x80109a90u) == 0x800b8280u);
    for (unsigned i = 0; i < 12; ++i) {
        check(fixture.get(0x8010424cu + i * 4u) ==
            static_cast<std::uint32_t>(static_cast<std::int32_t>(-300 + static_cast<int>(i))));
        check(fixture.get(0x8010427cu + i * 4u) == 200u + i);
    }
    for (unsigned i = 0; i < 10; ++i) {
        auto expected = i & 1u ? static_cast<std::int32_t>(1000 + i) : -1000 - static_cast<int>(i);
        check(fixture.get(EntityOutput + i * 4u) == static_cast<std::uint32_t>(expected));
    }
    check(fixture.get(0x800fa630u, 2) == 0 && fixture.get(0x800fa632u, 2) == 0);
    check(fixture.get(0x800fa634u, 2) == 0x2e00u && fixture.get(0x800fa636u, 2) == 0x55aau);
    check(fixture.get(0x800fa638u, 2) == 0xf95cu && fixture.get(0x800fa63au, 2) == 0);
    check(fixture.get(0x800fa63cu, 2) == 0 && fixture.get(Selector) == 0);
    check(fixture.get(0x80021498u, 2) == 1);
    check(fixture.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Stack);
    check(fixture.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        initial.gpr[NBA97_MATCH_INITIALIZE_RA].word);
    for (unsigned i = 0; i < 4; ++i)
        check(fixture.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + i].word ==
            initial.gpr[NBA97_MATCH_INITIALIZE_S0 + i].word);
    check(fixture.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0xcafebabeu);
    check(fixture.journal[0].pc == 0x80048d60u && fixture.journal[0].address == Stack - 0x18u);
    check(fixture.journal[4].pc == 0x80048d84u && fixture.journal[4].address == Stack - 8u);
    check(fixture.journal[fixture.progress.access_events - 1].pc == 0x80048fd4u);

    std::vector<std::uint32_t> access_pcs = {
        0x80048d60u,0x80048d68u,0x80048d70u,0x80048d78u,0x80048d84u,
        0x80048d8cu,0x80048d94u,0x80048d9cu,0x80048da4u};
    for (unsigned i = 0; i < 8; ++i) {
        access_pcs.push_back(0x80048da8u);
        if (!(i & 1u)) access_pcs.push_back(0x80048dbcu);
    }
    access_pcs.insert(access_pcs.end(), {0x80048ddcu,0x80048de4u,0x80048decu});
    for (unsigned i = 0; i < 12; ++i)
        access_pcs.insert(access_pcs.end(), {0x80048e14u,0x80048e1cu,0x80048e24u,
            0x80048e28u,0x80048e30u,0x80048e40u});
    for (unsigned i = 0; i < 10; ++i)
        access_pcs.insert(access_pcs.end(), {0x80048e60u,0x80048e6cu,0x80048e74u,
            0x80048e7cu,0x80048e84u});
    access_pcs.insert(access_pcs.end(), {0x80048ea8u,0x80048ec4u,0x80048ed0u,
        0x80048ee4u,0x80048eecu,0x80048ef4u,0x80048efcu,0x80048f04u,
        0x80048f1cu,0x80048f2cu,0x80048f58u,0x80048f74u,0x80048f84u,
        0x80048fb0u,0x80048fc4u,0x80048fc8u,0x80048fccu,0x80048fd0u,
        0x80048fd4u});
    check(access_pcs.size() == fixture.progress.access_events);
    for (std::size_t i = 0; i < access_pcs.size(); ++i) {
        check(fixture.journal[i].pc == access_pcs[i]);
        check(i == 0 || fixture.journal[i].operation > fixture.journal[i - 1].operation);
    }
}

void controller_and_roster_live_mutation() {
    Fixture controller;
    controller.mode = Fixture::ControllerMutation;
    check(controller.run() == NBA97_TEXT_COMPLETE);
    check(controller.progress.controller_iterations == 1 && controller.progress.controller_matches == 1);
    check(controller.get(0x800faba4u) == 0);
    check(controller.get(0x800fabd0u) == 9);

    Fixture negative;
    negative.mode = Fixture::ControllerNegative;
    check(negative.run() == NBA97_TEXT_COMPLETE);
    check(negative.progress.controller_iterations == 9);
    check(negative.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0);
    check(negative.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0);

    Fixture overwritten_unknown;
    overwritten_unknown.mode = Fixture::LastControllerUnknownBase;
    check(overwritten_unknown.run() == NBA97_TEXT_COMPLETE);
    check(overwritten_unknown.progress.controller_iterations == 8);
    check(overwritten_unknown.progress.completed);

    Fixture unknown_controller_counter;
    unknown_controller_counter.mode = Fixture::ControllerUnknownCounter;
    check(unknown_controller_counter.run() == NBA97_TEXT_UNKNOWN);
    check(unknown_controller_counter.progress.stopped_pc == 0x80048dc8u);
    check(unknown_controller_counter.progress.controller_iterations == 1);
    check(unknown_controller_counter.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
        0x800faba8u);
    check(unknown_controller_counter.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].known_mask == 0x0f);

    Fixture minus_one;
    minus_one.mode = Fixture::RosterMinusOne;
    check(minus_one.run() == NBA97_TEXT_COMPLETE);
    check(minus_one.progress.roster_iterations == 13);
    check(minus_one.get(0x8010427cu) == 200u);
    check(minus_one.get(0x801042acu) == 212u);

    Fixture runaway;
    runaway.mode = Fixture::RosterRunaway;
    runaway.context.operation_budget = 112;
    check(runaway.run() == NBA97_TEXT_LIMIT);
    check(runaway.progress.operations == 112 && runaway.progress.roster_iterations > 12);

    Fixture unknown_counter;
    unknown_counter.mode = Fixture::RosterUnknownCounter;
    check(unknown_counter.run() == NBA97_TEXT_UNKNOWN);
    check(unknown_counter.progress.stopped_pc == 0x80048e48u);
    check(unknown_counter.progress.roster_iterations == 1);
    check(unknown_counter.get(0x8010424cu) == static_cast<std::uint32_t>(-300));
    check(unknown_counter.get(0x8010427cu) == 200u);
    check(unknown_counter.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0x80104280u);
}

void signed_ids_knownness_and_access_failures() {
    Fixture partial;
    partial.known[partial.offset(0x80030000u)] = 0;
    check(partial.run() == NBA97_TEXT_COMPLETE);
    check(partial.get(0x8010424cu) == static_cast<std::uint32_t>(-300));
    auto destination = partial.offset(0x8010424cu);
    check(partial.known[destination] == 0 && partial.known[destination + 1] == 1);
    check(partial.known[destination + 2] == 1 && partial.known[destination + 3] == 1);
    auto half = std::find_if(partial.journal.begin(),
        partial.journal.begin() + static_cast<std::ptrdiff_t>(partial.progress.access_events),
        [](const auto& event) { return event.pc == 0x80048e1cu; });
    check(half != partial.journal.begin() + static_cast<std::ptrdiff_t>(partial.progress.access_events));
    check(half->known_mask == 2 && half->width == 2);

    Fixture unknown_pointer;
    unknown_pointer.known[unknown_pointer.offset(0x80020b8cu)] = 0;
    check(unknown_pointer.run() == NBA97_TEXT_UNKNOWN);
    check(unknown_pointer.progress.stopped_pc == 0x80048e1cu);
    check(unknown_pointer.progress.stores > 0 && unknown_pointer.progress.roster_iterations == 0);

    Fixture unaligned;
    unaligned.put(0x80020b8cu, 0x80030001u);
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
    check(unaligned.progress.stopped_pc == 0x80048e1cu && unaligned.progress.stopped_address == 0x80030001u);

    Fixture missing;
    missing.put(0x80020b8cu, 0x90000000u);
    check(missing.run() == NBA97_TEXT_RESOURCE);
    check(missing.progress.stopped_pc == 0x80048e1cu && missing.progress.stopped_address == 0x90000000u);

    Fixture bad_metadata;
    bad_metadata.known[bad_metadata.offset(0x800eb684u)] = 2;
    check(bad_metadata.run() == NBA97_TEXT_ARGUMENT);
    check(bad_metadata.progress.stopped_pc == 0x80048d8cu);
}

void selectors_reloads_and_callback_bases() {
    for (const std::uint32_t initial : {0u, 1u, 2u}) {
        Fixture fixture;
        fixture.put(Selector, initial);
        check(fixture.run() == NBA97_TEXT_COMPLETE);
        const auto first = initial == 0 ? 1u : 0u;
        const auto second = first == 0 ? 1u : 0u;
        check(fixture.calls[13].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x8002205cu + first * 0x14u);
        check(fixture.calls[14].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x80021eecu + first * 0x5cu);
        check(fixture.calls[15].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x8002205cu + second * 0x14u);
        check(fixture.calls[16].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x80021eecu + second * 0x5cu);
        check(fixture.get(Selector) == second);
    }

    Fixture mutation;
    mutation.mode = Fixture::SelectorMutation;
    check(mutation.run() == NBA97_TEXT_COMPLETE);
    check(mutation.calls[14].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x80021eecu + 7u * 0x5cu);
    check(mutation.calls[15].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x80022000u + 0x14u);
    check(mutation.calls[16].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x80021000u + 0x5cu);

    Fixture unknown_delay;
    unknown_delay.mode = Fixture::UnknownDisplayDelayBase;
    check(unknown_delay.run() == NBA97_TEXT_COMPLETE);
    check(unknown_delay.calls[15].registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 0x0f);
    check(unknown_delay.get(Selector) == 0u);

    Fixture unknown;
    unknown.known[unknown.offset(Selector)] = 0;
    check(unknown.run() == NBA97_TEXT_COMPLETE);
    check(unknown.get(0x800fa634u, 2) == 0x2e00u);
    check(unknown.get(0x80021498u, 2) == 1u);
    check(unknown.known[unknown.offset(Selector)] == 0);
    check(unknown.calls[13].registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 0x0f);
    check(unknown.calls[14].registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 0x0f);
}

void stack_relocation_wrap_and_unknowns() {
    Fixture relocated;
    relocated.mode = Fixture::RelocateStack;
    check(relocated.run() == NBA97_TEXT_COMPLETE);
    check(relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x8010fd28u);
    check(relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8fedcba0u);
    check(relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 0);
    check(relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 3].word == 0x33333333u);

    Fixture unknown_ra;
    unknown_ra.mode = Fixture::UnknownReturnAddress;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
    check(unknown_ra.progress.stopped_pc == 0x80048fdcu);
    check(unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x8010fd28u);
    check(unknown_ra.progress.restored_return_address.known_mask == 0x0e);

    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x0e;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN);
    check(unknown_sp.progress.operations == 0 && unknown_sp.progress.stopped_pc == 0x80048d60u);

    Fixture unknown_final_sp;
    unknown_final_sp.mode = Fixture::UnknownFinalStack;
    check(unknown_final_sp.run() == NBA97_TEXT_UNKNOWN);
    check(unknown_final_sp.progress.stopped_pc == 0x80048fc4u);
    check(unknown_final_sp.progress.callbacks_completed == 19);
    check(unknown_final_sp.get(0x80021498u, 2) == 1u);

    Fixture alignment;
    alignment.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word = Stack + 2u;
    check(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP);
    check(alignment.progress.stopped_pc == 0x80048d60u);

    Fixture wrapping;
    std::vector<std::uint8_t> high_bytes(16, 0), high_known(16, 1);
    std::vector<std::uint8_t> low_bytes(16, 0), low_known(16, 1);
    wrapping.regions.push_back({0xfffffff0u, high_bytes.data(), high_known.data(), high_bytes.size()});
    wrapping.regions.push_back({0u, low_bytes.data(), low_known.data(), low_bytes.size()});
    wrapping.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word = 0x10u;
    check(wrapping.run() == NBA97_TEXT_COMPLETE);
    check(wrapping.progress.frame_stack_pointer == 0xffffffe8u);
    check(wrapping.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x10u);
}

void entity_root_native_alias_reload() {
    Fixture fixture;
    std::uint8_t alias[4] = {};
    std::uint8_t alias_known[4] = {1,1,1,1};
    fixture.put(EntityTableRoot, 0x80040000u);
    for (unsigned i = 0; i < 4; ++i)
        alias[i] = fixture.bytes[fixture.offset(EntityTableRoot) + i];
    fixture.regions.clear();
    const auto pointer_offset = static_cast<std::size_t>(EntityTableRoot - Base);
    const auto output_offset = static_cast<std::size_t>(EntityOutput - Base);
    fixture.regions.push_back({Base, fixture.bytes.data(), fixture.known.data(), pointer_offset});
    fixture.regions.push_back({EntityTableRoot, alias, alias_known, 4});
    fixture.regions.push_back({EntityTableRoot + 4u, fixture.bytes.data() + pointer_offset + 4u,
        fixture.known.data() + pointer_offset + 4u, output_offset - pointer_offset - 4u});
    fixture.regions.push_back({EntityOutput, alias, alias_known, 4});
    fixture.regions.push_back({EntityOutput + 4u, fixture.bytes.data() + output_offset + 4u,
        fixture.known.data() + output_offset + 4u, fixture.bytes.size() - output_offset - 4u});
    check(fixture.run() == NBA97_TEXT_RESOURCE);
    check(fixture.progress.entity_iterations == 1);
    check(fixture.progress.stopped_pc == 0x80048e6cu);
    check(fixture.progress.stopped_address == 0xfffffc1cu);
}

void callback_failures_and_every_budget_prefix() {
    Fixture baseline;
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    const auto total = baseline.progress.operations;
    for (std::size_t budget = 0; budget < total; ++budget) {
        Fixture fixture;
        fixture.context.operation_budget = budget;
        check(fixture.run() == NBA97_TEXT_LIMIT);
        check(fixture.progress.operations == budget && !fixture.progress.completed);
        const auto next_operation = budget + 1;
        const auto next_access = std::find_if(baseline.journal.begin(),
            baseline.journal.begin() + static_cast<std::ptrdiff_t>(baseline.progress.access_events),
            [next_operation](const auto& event) { return event.operation == next_operation; });
        const auto next_call = std::find_if(baseline.calls.begin(), baseline.calls.end(),
            [next_operation](const auto& call) { return call.event.operation == next_operation; });
        check((next_access != baseline.journal.begin() +
            static_cast<std::ptrdiff_t>(baseline.progress.access_events)) !=
            (next_call != baseline.calls.end()));
        if (next_access != baseline.journal.begin() +
            static_cast<std::ptrdiff_t>(baseline.progress.access_events)) {
            check(fixture.progress.stopped_pc == next_access->pc);
            check(fixture.progress.stopped_address == next_access->address);
            check(fixture.progress.stopped_entry == 0);
        } else {
            check(fixture.progress.stopped_pc == next_call->event.pc);
            check(fixture.progress.stopped_address == 0);
            check(fixture.progress.stopped_entry == next_call->event.entry);
            for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
                check(fixture.progress.registers.gpr[i].word == next_call->registers.gpr[i].word);
                check(fixture.progress.registers.gpr[i].known_mask == next_call->registers.gpr[i].known_mask);
            }
        }
    }
    Fixture exact;
    exact.context.operation_budget = total;
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.operations == total);

    for (std::size_t call = 0; call < 19; ++call) {
        Fixture fixture;
        fixture.mode = Fixture::Refuse;
        fixture.refuse_at = call;
        check(fixture.run() == NBA97_TEXT_IO_REFUSED);
        check(fixture.progress.callbacks_completed == call);
        check(fixture.progress.stopped_entry == fixture.calls.back().event.entry);
        check(fixture.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            fixture.calls.back().event.pc + 8u);
    }
    Fixture malformed;
    malformed.mode = Fixture::Malformed;
    malformed.refuse_at = 9;
    check(malformed.run() == NBA97_TEXT_ARGUMENT);
    check(malformed.progress.callbacks_completed == 9);
    check(malformed.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x10);
}

void validation_guards() {
    Nba97GameSceneStartupProgress progress{};
    check(nba97_game_scene_startup(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture fixture;
    fixture.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(fixture.run() == NBA97_TEXT_ARGUMENT && fixture.progress.operations == 0);
    fixture.reset();
    fixture.context.access_journal = nullptr;
    fixture.context.access_journal_capacity = 1;
    check(fixture.run() == NBA97_TEXT_ARGUMENT);
    fixture.reset();
    fixture.regions.push_back(fixture.regions[0]);
    check(fixture.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    try {
        normal_path_and_exact_order();
        controller_and_roster_live_mutation();
        signed_ids_knownness_and_access_failures();
        selectors_reloads_and_callback_bases();
        stack_relocation_wrap_and_unknowns();
        entity_root_native_alias_reload();
        callback_failures_and_every_budget_prefix();
        validation_guards();
        std::cout << checks << " scene-startup owner checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << " after " << checks << " checks\n";
        return 1;
    }
}
