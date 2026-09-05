#include "recovered/game_scene_resources.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t Base = 0x80000000u;
constexpr std::size_t Size = 0x110000u;
constexpr std::uint32_t EntrySp = 0x8010ff80u;
constexpr std::uint32_t FrameSp = EntrySp - 0x20u;
constexpr std::uint32_t CallerRa = 0x81234560u;
std::size_t checks;

void check_at(bool condition, int line) {
    ++checks;
    if (!condition)
        throw std::runtime_error("scene-resources check failed at line " +
            std::to_string(line));
}
#define check(condition) check_at((condition), __LINE__)

struct Call {
    Nba97GameSceneResourcesEvent event{};
    Nba97GameSceneResourcesRegisters registers{};
};

struct Fixture {
    enum Mode {
        Ordinary,
        Refuse,
        MalformedMask,
        MalformedZero,
        MutateFlag,
        RelocateStack,
        RunawayTen,
        RunawayLetters,
        UnknownTenCursor,
        UnalignedTenCursor,
        MutateAwayCursor,
        MutateLetterRoot,
        MutateReleasePointers,
        PartialHomeLoader,
        UnknownTenCounter,
        UnknownLetterCounter,
        MutateUntouchedGpr,
        UnknownFinalSp,
        SignedTenOnce,
        SignedLettersOnce
    } mode = Ordinary;
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
    Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
    Nba97GameSceneResourcesContext context{};
    Nba97GameSceneResourcesProgress progress{};
    std::array<Nba97GameSceneResourcesAccess, 512> journal{};
    std::vector<Call> calls;
    std::size_t refuse_at = static_cast<std::size_t>(-1);
    std::uint32_t mutate_after_pc{};
    std::uint32_t mutate_flag_value{1};
    std::uint8_t mutate_flag_mask{0x0f};
    bool alternate{};

    explicit Fixture(bool use_alternate = false) : alternate(use_alternate) {
        context.memory = {&region, 1};
        context.operation_budget = 10000;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        seed();
    }

    std::size_t offset(std::uint32_t address) const {
        check(address >= Base && address - Base < bytes.size());
        return address - Base;
    }
    void put(std::uint32_t address, std::uint32_t value,
        std::uint8_t known_mask = 0x0f) {
        const auto at = offset(address);
        for (unsigned i = 0; i < 4; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = static_cast<std::uint8_t>((known_mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes[at + i]) << (i * 8u);
        return value;
    }
    void seed() {
        put(0x800eb678u, alternate ? 1u : 0u);
        put(0x80021d74u, 2u);
        put(0x80021d78u, 3u);
        put(0x800b7394u + 2u * 4u, 0x80027000u);
        put(0x800b741cu + 3u * 4u, 0x80027100u);
        put(0x800faba0u, 0x9100aba0u);
        put(0x80102918u, 0x91102918u);
    }
    static std::uint32_t child_result(
        const Nba97GameSceneResourcesEvent& event,
        const Nba97GameSceneResourcesRegisters& registers) {
        const auto a0 = registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;
        const auto a1 = registers.gpr[NBA97_MATCH_INITIALIZE_A1].word;
        if (event.entry == 0x80029bfcu)
            return 0x90000000u ^ a0;
        if (event.entry == 0x800a3fecu)
            return a0 + a1 * 0x100u + (event.pc & 0xffu);
        if (event.entry == 0x80090160u)
            return 0xa0000000u ^ a0 ^ a1;
        return 0xb0000000u ^ event.pc;
    }
    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameSceneResourcesEvent* event,
        Nba97GameSceneResourcesRegisters* registers) {
        auto& fixture = *static_cast<Fixture*>(opaque);
        fixture.calls.push_back({*event, *registers});
        const auto index = fixture.calls.size() - 1u;
        if (fixture.mode == Refuse && index == fixture.refuse_at)
            return 0;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
            child_result(*event, *registers), 0x0f};
        if (fixture.mode == MalformedMask && index == fixture.refuse_at)
            registers->gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
        if (fixture.mode == MalformedZero && index == fixture.refuse_at)
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
        if (fixture.mode == MutateFlag &&
            event->pc == fixture.mutate_after_pc)
            fixture.put(0x800eb678u, fixture.mutate_flag_value,
                fixture.mutate_flag_mask);
        if (fixture.mode == RelocateStack && event->pc == 0x800530d0u) {
            constexpr std::uint32_t relocated = 0x8010fe00u;
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {relocated, 0x0f};
            fixture.put(relocated + 0x1cu, 0x8fedcba0u);
            fixture.put(relocated + 0x18u, 0x22222222u);
            fixture.put(relocated + 0x14u, 0x11111111u);
            fixture.put(relocated + 0x10u, 0x00000000u);
        }
        if (fixture.mode == RunawayTen && event->pc == 0x80052dacu)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0f};
        if (fixture.mode == RunawayLetters && event->pc == 0x80052e90u)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0f};
        if (fixture.mode == UnknownTenCursor && event->pc == 0x80052d90u)
            registers->gpr[GPR_S1] = {0x800fac24u, 0x07};
        if (fixture.mode == UnalignedTenCursor && event->pc == 0x80052d90u)
            registers->gpr[GPR_S1] = {0x800fac25u, 0x0f};
        if (fixture.mode == MutateAwayCursor && event->pc == 0x80052dacu)
            registers->gpr[18] = {0x80001000u, 0x0f};
        if (fixture.mode == MutateLetterRoot && event->pc == 0x80052e90u &&
            fixture.calls.size() == 30u)
            registers->gpr[18] = {0x12345678u, 0x0f};
        if (fixture.mode == MutateReleasePointers) {
            if (event->pc == 0x80052fa4u) {
                fixture.put(0x800faba0u, 0x92222222u);
                fixture.put(0x800dcbe8u, 0x93333333u);
                fixture.put(0x800fdb34u, 0x94444444u);
            }
            if (event->pc == 0x8005300cu) {
                fixture.put(0x80102918u, 0x95555555u);
                fixture.put(0x800f9fc0u, 0x96666666u);
            }
        }
        if (fixture.mode == PartialHomeLoader && event->pc == 0x80052d08u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 0x05;
        if (fixture.mode == UnknownTenCounter && event->pc == 0x80052dacu)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {1, 0x07};
        if (fixture.mode == UnknownLetterCounter && event->pc == 0x80052e90u)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x07};
        if (fixture.mode == MutateUntouchedGpr && event->pc == 0x80052c30u)
            registers->gpr[15] = {0xdeadbeefu, 0x05};
        if (fixture.mode == UnknownFinalSp && event->pc == 0x800530d0u)
            registers->gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
        if (fixture.mode == SignedTenOnce && event->pc == 0x80052dacu &&
            fixture.calls.size() == 10u)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0xffffffffu, 0x0f};
        if (fixture.mode == SignedLettersOnce && event->pc == 0x80052e90u &&
            fixture.calls.size() == 30u)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0xfffffffeu, 0x0f};
        return 1;
    }
    int run() { return nba97_game_scene_resources(&context, &progress); }

    static constexpr unsigned GPR_S1 = 17;
};

std::vector<std::uint32_t> normal_call_pcs() {
    std::vector<std::uint32_t> result = {0x80052c30u,0x80052c68u,
        0x80052c8cu,0x80052cd0u,0x80052d08u,0x80052d34u,
        0x80052d50u,0x80052d78u};
    for (unsigned i = 0; i < 10; ++i) {
        result.push_back(0x80052d90u);
        result.push_back(0x80052dacu);
    }
    result.push_back(0x80052dccu);
    for (unsigned i = 0; i < 26; ++i) result.push_back(0x80052e90u);
    const std::uint32_t tail[] = {0x80052eccu,0x80052edcu,0x80052f2cu,
        0x80052f48u,0x80052f68u,0x80052f94u,0x80052fa4u,
        0x80052fc8u,0x80052fd8u,0x80052fe8u,0x80053004u,
        0x8005300cu,0x8005301cu,0x8005302cu,0x80053048u,
        0x800530c0u,0x800530d0u};
    result.insert(result.end(), std::begin(tail), std::end(tail));
    return result;
}

std::vector<std::uint32_t> alternate_call_pcs() {
    std::vector<std::uint32_t> result = {0x80052c30u,0x80052cb0u,
        0x80052cb8u,0x80052cc0u,0x80052de8u,0x80052e18u,
        0x80052e2cu};
    for (unsigned i = 0; i < 10; ++i) result.push_back(0x80052e48u);
    result.push_back(0x80052e68u);
    result.push_back(0x80052e78u);
    for (unsigned i = 0; i < 26; ++i) result.push_back(0x80052e90u);
    const std::uint32_t tail[] = {0x80052ef0u,0x80052f00u,
        0x80052f10u,0x80052f18u,0x80052f2cu,0x80052f48u,
        0x80052f7cu,0x80052f84u,0x80052f8cu,0x80052f94u,
        0x80052fa4u,0x80052fd8u,0x80052fe8u,0x8005305cu,
        0x8005306cu,0x80053094u,0x8005309cu,0x800530a4u};
    result.insert(result.end(), std::begin(tail), std::end(tail));
    return result;
}

std::uint32_t expected_entry(std::uint32_t pc) {
    switch (pc) {
    case 0x80052c30u: return 0x800536a0u;
    case 0x80052c68u: return 0x8004d490u;
    case 0x80052c8cu: case 0x80052d08u: case 0x80052d34u:
    case 0x80052dccu: case 0x80052e18u: case 0x80052e68u:
    case 0x80052eccu: case 0x80052f00u: case 0x8005306cu:
    case 0x800530d0u: return 0x80029bfcu;
    case 0x80052cb0u: case 0x80052de8u: case 0x80052ef0u:
    case 0x80052f7cu: case 0x8005305cu: return 0x80029bccu;
    case 0x80052cb8u: case 0x80052cd0u: return 0x800516e4u;
    case 0x80052cc0u: case 0x80052e78u: case 0x80052f18u:
    case 0x80052f8cu: case 0x800530a4u: return 0x80029bd4u;
    case 0x80052d50u: case 0x80052d78u: case 0x80052d90u:
    case 0x80052dacu: case 0x80052e2cu: case 0x80052e48u:
    case 0x80052e90u: return 0x800a3fecu;
    case 0x80052edcu: case 0x80052f10u: return 0x80051294u;
    case 0x80052f2cu: case 0x80052f48u: return 0x80090160u;
    case 0x80052f68u: return 0x8004dc08u;
    case 0x80052f84u: return 0x8004fd38u;
    case 0x80052f94u: case 0x8005300cu: return 0x800994f4u;
    case 0x80052fa4u: case 0x80052fc8u: case 0x80052fd8u:
    case 0x80052fe8u: case 0x8005301cu: case 0x8005302cu:
        return 0x80090698u;
    case 0x80053004u: return 0x8004fd48u;
    case 0x80053048u: return 0x800504a8u;
    case 0x80053094u: return 0x80050dd0u;
    case 0x8005309cu: return 0x80050dc8u;
    default: return 0x800479b8u;
    }
}

std::uint8_t expected_arguments(std::uint32_t entry) {
    if (entry == 0x8004d490u || entry == 0x800994f4u ||
        entry == 0x80090698u || entry == 0x8004fd48u)
        return 1;
    if (entry == 0x80029bfcu || entry == 0x800a3fecu)
        return 2;
    if (entry == 0x80029bccu || entry == 0x80090160u)
        return 3;
    return 0;
}

std::uint8_t expected_kind(std::uint32_t entry) {
    switch (entry) {
    case 0x800536a0u: return NBA97_GAME_SCENE_RESOURCES_CHILD_800536A0;
    case 0x8004d490u: return NBA97_GAME_SCENE_RESOURCES_CHILD_8004D490;
    case 0x80029bfcu: return NBA97_GAME_SCENE_RESOURCES_CHILD_80029BFC;
    case 0x80029bccu: return NBA97_GAME_SCENE_RESOURCES_CHILD_80029BCC;
    case 0x800516e4u: return NBA97_GAME_SCENE_RESOURCES_CHILD_800516E4;
    case 0x80029bd4u: return NBA97_GAME_SCENE_RESOURCES_CHILD_80029BD4;
    case 0x800a3fecu: return NBA97_GAME_SCENE_RESOURCES_CHILD_800A3FEC;
    case 0x80051294u: return NBA97_GAME_SCENE_RESOURCES_CHILD_80051294;
    case 0x80090160u: return NBA97_GAME_SCENE_RESOURCES_CHILD_80090160;
    case 0x8004dc08u: return NBA97_GAME_SCENE_RESOURCES_CHILD_8004DC08;
    case 0x8004fd38u: return NBA97_GAME_SCENE_RESOURCES_CHILD_8004FD38;
    case 0x800994f4u: return NBA97_GAME_SCENE_RESOURCES_CHILD_800994F4;
    case 0x80090698u: return NBA97_GAME_SCENE_RESOURCES_CHILD_80090698;
    case 0x8004fd48u: return NBA97_GAME_SCENE_RESOURCES_CHILD_8004FD48;
    case 0x800504a8u: return NBA97_GAME_SCENE_RESOURCES_CHILD_800504A8;
    case 0x80050dd0u: return NBA97_GAME_SCENE_RESOURCES_CHILD_80050DD0;
    case 0x80050dc8u: return NBA97_GAME_SCENE_RESOURCES_CHILD_80050DC8;
    default: return NBA97_GAME_SCENE_RESOURCES_CHILD_800479B8;
    }
}

void check_call_sequence(const Fixture& fixture,
    const std::vector<std::uint32_t>& expected) {
    check(fixture.calls.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        check(fixture.calls[i].event.pc == expected[i]);
        check(fixture.calls[i].event.delay_slot_pc == expected[i] + 4u);
        check(fixture.calls[i].event.entry == expected_entry(expected[i]));
        check(fixture.calls[i].event.argument_count ==
            expected_arguments(fixture.calls[i].event.entry));
        check(fixture.calls[i].event.kind ==
            expected_kind(fixture.calls[i].event.entry));
        check(fixture.calls[i].event.operation >= 1u);
        check(fixture.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            expected[i] + 8u);
        check(fixture.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 0x0f);
    }
}

void normal_path_resources_and_order() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 182 && f.progress.accesses == 110 &&
        f.progress.reads == 46 && f.progress.stores == 64 &&
        f.progress.callbacks_completed == 72 &&
        f.progress.access_events == 110);
    check_call_sequence(f, normal_call_pcs());
    check(f.get(0x800b72dcu) == 1u && f.get(0x800fb820u) == 0u &&
        f.get(0x800fac20u) == 0xfffffffdu);
    const auto home = Fixture::child_result(f.calls[4].event,
        f.calls[4].registers);
    const auto away = Fixture::child_result(f.calls[5].event,
        f.calls[5].registers);
    check(f.get(0x800f0edcu) == home && f.get(0x800f0facu) == away);
    check(f.get(0x800ebc38u) ==
        Fixture::child_result(f.calls[6].event, f.calls[6].registers));
    check(f.get(0x800f0f64u) ==
        Fixture::child_result(f.calls[7].event, f.calls[7].registers));
    for (unsigned i = 0; i < 10; ++i) {
        check(f.get(0x800fac24u + i * 4u) ==
            Fixture::child_result(f.calls[8 + i * 2].event,
                f.calls[8 + i * 2].registers));
        check(f.get(0x800fb154u + i * 4u) ==
            Fixture::child_result(f.calls[9 + i * 2].event,
                f.calls[9 + i * 2].registers));
    }
    const auto letters = Fixture::child_result(f.calls[28].event,
        f.calls[28].registers);
    check(f.get(0x800fabccu) == letters);
    for (unsigned i = 0; i < 26; ++i)
        check(f.get(0x800feca8u + i * 4u) ==
            Fixture::child_result(f.calls[29 + i].event,
                f.calls[29 + i].registers));
    check(f.get(0x800d9284u) == 0u);
    check(f.get(0x801041a0u) ==
        Fixture::child_result(f.calls[55].event, f.calls[55].registers));
    check(f.get(0x800fdb34u) ==
        Fixture::child_result(f.calls[57].event, f.calls[57].registers));
    check(f.get(0x800dcbe8u) ==
        Fixture::child_result(f.calls[58].event, f.calls[58].registers));
    check(f.get(0x80103f44u) ==
        Fixture::child_result(f.calls[71].event, f.calls[71].registers));
    check(f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x8002639cu &&
        f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x20u);
    check(f.calls[57].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x800263dcu &&
        f.calls[57].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x970u &&
        f.calls[57].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0u);
    check(f.calls[58].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x800263e8u &&
        f.calls[58].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x210u);
    check(f.calls[60].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0u);
    check(f.calls[61].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.get(0x801041a0u));
    check(f.calls[62].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x9100aba0u);
    check(f.calls[63].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.get(0x800dcbe8u));
    check(f.calls[64].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.get(0x800fdb34u));
    check(f.calls[65].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 700u);
    check(f.calls[66].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0u);
    check(f.calls[67].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x91102918u);
    check(f.progress.frame_stack_pointer == FrameSp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == CallerRa &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 0x11000010u &&
        f.progress.registers.gpr[17].word == 0x11000011u &&
        f.progress.registers.gpr[18].word == 0x11000012u);
}

void exact_normal_access_sequence() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE);
    std::vector<std::uint32_t> expected = {0x80052c24u,0x80052c28u,
        0x80052c2cu,0x80052c34u,0x80052c3cu,0x80052c48u,
        0x80052c54u,0x80052c5cu,0x80052c74u,0x80052c98u,
        0x80052ca0u,0x80052cdcu,0x80052cf0u,0x80052d04u,
        0x80052d14u,0x80052d28u,0x80052d30u,0x80052d40u,
        0x80052d4cu,0x80052d5cu,0x80052d74u,0x80052d84u};
    for (unsigned i = 0; i < 10; ++i) {
        expected.push_back(0x80052d8cu); expected.push_back(0x80052d9cu);
        expected.push_back(0x80052da4u); expected.push_back(0x80052db4u);
    }
    expected.push_back(0x80052dd8u);
    for (unsigned i = 0; i < 26; ++i) expected.push_back(0x80052e98u);
    const std::uint32_t tail[] = {0x80052eb0u,0x80052eb8u,
        0x80052ed8u,0x80052f44u,0x80052f54u,0x80052f5cu,
        0x80052fa0u,0x80052fb0u,0x80052fc4u,0x80052fd4u,
        0x80052fe4u,0x80052ff4u,0x80053018u,0x80053028u,
        0x80053038u,0x800530b0u,0x800530dcu,0x800530e0u,
        0x800530e4u,0x800530e8u,0x800530ecu};
    expected.insert(expected.end(), std::begin(tail), std::end(tail));
    check(expected.size() == f.progress.access_events);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        check(f.journal[i].pc == expected[i]);
        check(f.journal[i].width == 4);
    }
}

void alternate_path_and_publications() {
    Fixture f(true);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 142 && f.progress.accesses == 79 &&
        f.progress.reads == 26 && f.progress.stores == 53 &&
        f.progress.callbacks_completed == 63);
    check_call_sequence(f, alternate_call_pcs());
    check(f.get(0x800f0edcu) != 0xcdcdcdcdu);
    check(f.get(0x800f0facu) == 0xcdcdcdcdu);
    for (unsigned i = 0; i < 10; ++i) {
        check(f.get(0x800fac24u + i * 4u) != 0xcdcdcdcdu);
        check(f.get(0x800fb154u + i * 4u) == 0xcdcdcdcdu);
    }
    const auto fat = Fixture::child_result(f.calls[59].event,
        f.calls[59].registers);
    check(f.get(0x801063c4u) == fat && f.get(0x800f0ed8u) == fat &&
        f.get(0x800f0ed4u) == fat);
    check(f.get(0x80103f44u) == 0xcdcdcdcdu);
    check(f.calls[45].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 700u &&
        f.calls[45].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 480u &&
        f.calls[45].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0u);
    check(f.calls[54].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0u);
    check(f.calls[55].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.get(0x801041a0u));
    check(f.calls[56].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.get(0x800dcbe8u));
    check(f.calls[57].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.get(0x800fdb34u));

    std::vector<std::uint32_t> access = {0x80052c24u,0x80052c28u,
        0x80052c2cu,0x80052c34u,0x80052c3cu,0x80052c48u,
        0x80052c54u,0x80052c5cu,0x80052cdcu,0x80052df4u,
        0x80052e08u,0x80052e28u,0x80052e38u};
    for (unsigned i = 0; i < 10; ++i) {
        access.push_back(0x80052e40u);
        access.push_back(0x80052e50u);
    }
    access.push_back(0x80052e74u);
    for (unsigned i = 0; i < 26; ++i) access.push_back(0x80052e98u);
    const std::uint32_t tail_access[] = {0x80052eb0u,0x80052eb8u,
        0x80052f0cu,0x80052f44u,0x80052f54u,0x80052f5cu,
        0x80052fa0u,0x80052fb0u,0x80052fd4u,0x80052fe4u,
        0x80052ff4u,0x80053080u,0x80053088u,0x80053090u,
        0x800530b0u,0x800530e0u,0x800530e4u,0x800530e8u,
        0x800530ecu};
    access.insert(access.end(), std::begin(tail_access), std::end(tail_access));
    check(access.size() == f.progress.access_events);
    for (std::size_t i = 0; i < access.size(); ++i)
        check(f.journal[i].pc == access[i]);
}

void every_live_flag_reload_can_redirect() {
    struct Case { std::uint32_t mutation_pc; std::uint32_t expected_pc; };
    const Case cases[] = {
        {0x80052c30u,0x80052cb0u}, {0x80052c68u,0x80052cb0u},
        {0x80052c8cu,0x80052cb0u}, {0x80052cd0u,0x80052de8u},
        {0x80052e90u,0x80052ef0u}, {0x80052f48u,0x80052f7cu},
        {0x80052fa4u,0x80052fd8u}, {0x80052fe8u,0x8005305cu},
        {0x8005302cu,0x8005305cu}, {0x80053048u,0u}
    };
    for (const auto& item : cases) {
        Fixture f;
        f.mode = Fixture::MutateFlag;
        f.mutate_after_pc = item.mutation_pc;
        check(f.run() == NBA97_TEXT_COMPLETE);
        const auto found = std::find_if(f.calls.begin(), f.calls.end(),
            [&](const Call& call) { return call.event.pc == item.expected_pc; });
        if (item.expected_pc)
            check(found != f.calls.end());
        else {
            check(std::none_of(f.calls.begin(), f.calls.end(),
                [](const Call& call) { return call.event.pc == 0x800530c0u; }));
            check(std::none_of(f.calls.begin(), f.calls.end(),
                [](const Call& call) { return call.event.pc == 0x800530d0u; }));
        }
    }

    struct UnknownCase { std::uint32_t mutation_pc; std::uint32_t branch_pc; };
    const UnknownCase unknown_cases[] = {
        {0x80052c30u,0x80052c60u}, {0x80052c68u,0x80052c7cu},
        {0x80052c8cu,0x80052ca4u}, {0x80052cd0u,0x80052ce4u},
        {0x80052e90u,0x80052ebcu}, {0x80052f48u,0x80052f60u},
        {0x80052fa4u,0x80052fb8u}, {0x80052fe8u,0x80052ffcu},
        {0x8005302cu,0x80053040u}, {0x80053048u,0x800530b8u}
    };
    for (const auto& item : unknown_cases) {
        Fixture f;
        f.mode = Fixture::MutateFlag;
        f.mutate_after_pc = item.mutation_pc;
        f.mutate_flag_value = 0;
        f.mutate_flag_mask = 0x07;
        check(f.run() == NBA97_TEXT_UNKNOWN &&
            f.progress.stopped_pc == item.branch_pc);
    }
}

void delay_slots_loops_stack_and_unknownness() {
    Fixture adjusted_lui;
    adjusted_lui.context.operation_budget = 5;
    check(adjusted_lui.run() == NBA97_TEXT_LIMIT &&
        adjusted_lui.progress.stopped_pc == 0x80052c3cu &&
        adjusted_lui.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0x800f0000u);

    Fixture runaway_ten;
    runaway_ten.mode = Fixture::RunawayTen;
    runaway_ten.context.operation_budget = 90;
    check(runaway_ten.run() == NBA97_TEXT_LIMIT &&
        runaway_ten.progress.operations == 90 &&
        runaway_ten.progress.stopped_pc != 0);
    Fixture runaway_letters;
    runaway_letters.mode = Fixture::RunawayLetters;
    runaway_letters.context.operation_budget = 170;
    check(runaway_letters.run() == NBA97_TEXT_LIMIT &&
        runaway_letters.progress.operations == 170);
    Fixture signed_ten;
    signed_ten.mode = Fixture::SignedTenOnce;
    check(signed_ten.run() == NBA97_TEXT_COMPLETE &&
        std::count_if(signed_ten.calls.begin(), signed_ten.calls.end(),
            [](const Call& call) { return call.event.pc == 0x80052d90u; }) == 12);
    Fixture signed_letters;
    signed_letters.mode = Fixture::SignedLettersOnce;
    check(signed_letters.run() == NBA97_TEXT_COMPLETE &&
        std::count_if(signed_letters.calls.begin(), signed_letters.calls.end(),
            [](const Call& call) { return call.event.pc == 0x80052e90u; }) == 28);

    Fixture unknown_counter;
    unknown_counter.mode = Fixture::UnknownTenCounter;
    check(unknown_counter.run() == NBA97_TEXT_UNKNOWN &&
        unknown_counter.progress.stopped_pc == 0x80052dbcu &&
        unknown_counter.progress.registers.gpr[18].word == 0x800fb158u &&
        unknown_counter.get(0x800fb154u) != 0xcdcdcdcdu);
    Fixture unknown_letter_counter;
    unknown_letter_counter.mode = Fixture::UnknownLetterCounter;
    check(unknown_letter_counter.run() == NBA97_TEXT_UNKNOWN &&
        unknown_letter_counter.progress.stopped_pc == 0x80052ea4u &&
        unknown_letter_counter.progress.registers.gpr[17].word == 0x800fecacu &&
        unknown_letter_counter.get(0x800feca8u) != 0xcdcdcdcdu);

    Fixture unknown_cursor;
    unknown_cursor.mode = Fixture::UnknownTenCursor;
    check(unknown_cursor.run() == NBA97_TEXT_UNKNOWN &&
        unknown_cursor.progress.stopped_pc == 0x80052da4u);
    Fixture unaligned_cursor;
    unaligned_cursor.mode = Fixture::UnalignedTenCursor;
    check(unaligned_cursor.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_cursor.progress.stopped_pc == 0x80052da4u &&
        unaligned_cursor.progress.stopped_address == 0x800fac25u);

    Fixture away_cursor;
    away_cursor.mode = Fixture::MutateAwayCursor;
    check(away_cursor.run() == NBA97_TEXT_COMPLETE &&
        away_cursor.get(0x80001000u) != 0xcdcdcdcdu);
    Fixture letter_root;
    letter_root.mode = Fixture::MutateLetterRoot;
    check(letter_root.run() == NBA97_TEXT_COMPLETE);
    const auto second_letter = std::find_if(letter_root.calls.begin(),
        letter_root.calls.end(), [](const Call& call) {
            return call.event.pc == 0x80052e90u &&
                call.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 1u;
        });
    check(second_letter != letter_root.calls.end() &&
        second_letter->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x12345678u);

    Fixture release_reloads;
    release_reloads.mode = Fixture::MutateReleasePointers;
    check(release_reloads.run() == NBA97_TEXT_COMPLETE);
    check(release_reloads.calls[62].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x92222222u &&
        release_reloads.calls[63].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x93333333u &&
        release_reloads.calls[64].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x94444444u &&
        release_reloads.calls[67].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x95555555u &&
        release_reloads.calls[68].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x96666666u);

    Fixture partial;
    partial.mode = Fixture::PartialHomeLoader;
    check(partial.run() == NBA97_TEXT_COMPLETE);
    check(partial.calls[6].registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0x05);
    const auto root_offset = partial.offset(0x800f0edcu);
    check(partial.known[root_offset] == 1 && partial.known[root_offset + 1] == 0 &&
        partial.known[root_offset + 2] == 1 && partial.known[root_offset + 3] == 0);

    Fixture prefix_baseline;
    check(prefix_baseline.run() == NBA97_TEXT_COMPLETE);
    const auto optional_read = std::find_if(prefix_baseline.journal.begin(),
        prefix_baseline.journal.begin() +
            static_cast<std::ptrdiff_t>(prefix_baseline.progress.access_events),
        [](const auto& event) { return event.pc == 0x80052fc4u; });
    check(optional_read != prefix_baseline.journal.begin() +
        static_cast<std::ptrdiff_t>(prefix_baseline.progress.access_events));
    Fixture optional_prefix;
    optional_prefix.context.operation_budget = optional_read->operation - 1u;
    check(optional_prefix.run() == NBA97_TEXT_LIMIT &&
        optional_prefix.progress.stopped_pc == 0x80052fc4u &&
        optional_prefix.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x80100000u);

    Fixture wrapping;
    wrapping.put(0x80021d74u, 0x40000002u);
    check(wrapping.run() == NBA97_TEXT_COMPLETE);
    check(wrapping.journal[13].address == 0x800b739cu);

    Fixture unknown_index;
    unknown_index.put(0x80021d74u, 2u, 0x07);
    check(unknown_index.run() == NBA97_TEXT_UNKNOWN &&
        unknown_index.progress.stopped_pc == 0x80052d04u);
    check(unknown_index.progress.operations == 17);

    Fixture unknown_flag;
    unknown_flag.put(0x800eb678u, 0u, 0x07);
    check(unknown_flag.run() == NBA97_TEXT_UNKNOWN &&
        unknown_flag.progress.stopped_pc == 0x80052c60u &&
        unknown_flag.progress.operations == 9 &&
        unknown_flag.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 700u);

    Fixture relocated;
    relocated.mode = Fixture::RelocateStack;
    check(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x8010fe20u &&
        relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8fedcba0u &&
        relocated.progress.registers.gpr[18].word == 0x22222222u &&
        relocated.progress.registers.gpr[17].word == 0x11111111u &&
        relocated.progress.registers.gpr[16].word == 0u);
    Fixture live_gpr;
    live_gpr.mode = Fixture::MutateUntouchedGpr;
    check(live_gpr.run() == NBA97_TEXT_COMPLETE && live_gpr.calls.size() == 72 &&
        live_gpr.calls[1].registers.gpr[15].word == 0xdeadbeefu &&
        live_gpr.calls[1].registers.gpr[15].known_mask == 0x05 &&
        live_gpr.progress.registers.gpr[15].word == 0xdeadbeefu &&
        live_gpr.progress.registers.gpr[15].known_mask == 0x05);
    Fixture unknown_ra;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0x07;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x800530f4u &&
        unknown_ra.progress.operations == 182 &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp);
    Fixture partial_saved;
    partial_saved.context.registers.gpr[18].known_mask = 0x05;
    check(partial_saved.run() == NBA97_TEXT_COMPLETE &&
        partial_saved.progress.restored_saved_register[0].word == 0x11000012u &&
        partial_saved.progress.restored_saved_register[0].known_mask == 0x05 &&
        partial_saved.progress.registers.gpr[18].known_mask == 0x05);
    Fixture unknown_final_sp;
    unknown_final_sp.mode = Fixture::UnknownFinalSp;
    check(unknown_final_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_final_sp.progress.stopped_pc == 0x800530e0u &&
        unknown_final_sp.progress.operations == 178 &&
        unknown_final_sp.get(0x80103f44u) != 0xcdcdcdcdu);
}

void refusal_malformed_and_every_budget_prefix() {
    for (unsigned path = 0; path < 2; ++path) {
        Fixture baseline(path != 0);
        const auto initial = baseline.bytes;
        check(baseline.run() == NBA97_TEXT_COMPLETE);
        for (std::size_t budget = 0; budget < baseline.progress.operations; ++budget) {
            Fixture f(path != 0);
            f.context.operation_budget = budget;
            check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
                !f.progress.completed);
            const auto next_operation = budget + 1u;
            const auto access = std::find_if(baseline.journal.begin(),
                baseline.journal.begin() + static_cast<std::ptrdiff_t>(baseline.progress.access_events),
                [&](const auto& event) { return event.operation == next_operation; });
            const auto call = std::find_if(baseline.calls.begin(), baseline.calls.end(),
                [&](const Call& item) { return item.event.operation == next_operation; });
            check((access != baseline.journal.begin() +
                static_cast<std::ptrdiff_t>(baseline.progress.access_events)) !=
                (call != baseline.calls.end()));
            if (access != baseline.journal.begin() +
                static_cast<std::ptrdiff_t>(baseline.progress.access_events)) {
                check(f.progress.stopped_pc == access->pc &&
                    f.progress.stopped_address == access->address &&
                    f.progress.stopped_entry == 0);
            } else {
                check(f.progress.stopped_pc == call->event.pc &&
                    f.progress.stopped_entry == call->event.entry);
                for (unsigned reg = 0; reg < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++reg) {
                    check(f.progress.registers.gpr[reg].word == call->registers.gpr[reg].word &&
                        f.progress.registers.gpr[reg].known_mask == call->registers.gpr[reg].known_mask);
                }
            }
            auto expected = initial;
            for (std::size_t i = 0; i < baseline.progress.access_events; ++i) {
                const auto& event = baseline.journal[i];
                if (event.operation > budget ||
                    event.kind != NBA97_GAME_SCENE_RESOURCES_STORE)
                    continue;
                for (unsigned byte = 0; byte < 4; ++byte)
                    expected[event.address - Base + byte] =
                        static_cast<std::uint8_t>(event.value >> (byte * 8u));
            }
            check(f.bytes == expected);
        }
        Fixture exact(path != 0);
        exact.context.operation_budget = baseline.progress.operations;
        check(exact.run() == NBA97_TEXT_COMPLETE);

        for (std::size_t call = 0; call < baseline.calls.size(); ++call) {
            Fixture f(path != 0);
            f.mode = Fixture::Refuse;
            f.refuse_at = call;
            check(f.run() == NBA97_TEXT_IO_REFUSED &&
                f.progress.callbacks_completed == call &&
                f.progress.stopped_pc == f.calls.back().event.pc &&
                f.progress.stopped_entry == f.calls.back().event.entry);
        }
    }
    Fixture malformed;
    malformed.mode = Fixture::MalformedMask;
    malformed.refuse_at = 7;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 7);
    Fixture bad_zero;
    bad_zero.mode = Fixture::MalformedZero;
    bad_zero.refuse_at = 2;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT &&
        bad_zero.progress.callbacks_completed == 2);
}

void validation_mapping_alignment_alias() {
    Nba97GameSceneResourcesProgress progress{};
    check(nba97_game_scene_resources(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_scene_resources(&null_out.context, nullptr) == NBA97_TEXT_ARGUMENT);
    Fixture no_io;
    no_io.context.io = nullptr;
    check(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations == 5 && no_io.progress.callbacks_completed == 0);
    Fixture unaligned;
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80052c24u);
    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x80052c24u &&
        unknown_sp.progress.operations == 0 &&
        unknown_sp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask == 0x07);
    Fixture stack_wrap;
    std::array<std::uint8_t, 16> low_stack{};
    std::array<std::uint8_t, 16> low_known{};
    low_known.fill(1);
    Nba97GameTextRegion wrap_regions[2] = {
        {0u, low_stack.data(), low_known.data(), low_stack.size()},
        stack_wrap.region};
    stack_wrap.context.memory = {wrap_regions, 2};
    stack_wrap.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    check(stack_wrap.run() == NBA97_TEXT_COMPLETE &&
        stack_wrap.progress.frame_stack_pointer == 0xfffffff0u &&
        stack_wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x10u &&
        stack_wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == CallerRa);
    Fixture missing;
    missing.region.size = missing.offset(FrameSp + 0x1cu);
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x80052c24u);
    Fixture malformed_known;
    malformed_known.known[malformed_known.offset(FrameSp + 0x1cu)] = 2;
    check(malformed_known.run() == NBA97_TEXT_ARGUMENT &&
        malformed_known.progress.operations == 1);
    Fixture no_masks;
    no_masks.region.known = nullptr;
    no_masks.context.registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask = 0x07;
    check(no_masks.run() == NBA97_TEXT_ARGUMENT &&
        no_masks.progress.stopped_pc == 0x80052c34u);
    Fixture overlap;
    Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
    overlap.context.memory = {regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
    Fixture empty_region;
    empty_region.region.size = 0;
    check(empty_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_data;
    null_data.region.data = nullptr;
    check(null_data.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_regions;
    null_regions.context.memory = {nullptr, 1};
    check(null_regions.run() == NBA97_TEXT_ARGUMENT);
    Fixture wrapped_region;
    wrapped_region.region.base = 0xfffffffcu;
    wrapped_region.region.size = 8;
    check(wrapped_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_zero_reg;
    bad_zero_reg.context.registers.gpr[0].word = 1;
    check(bad_zero_reg.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_journal;
    bad_journal.context.access_journal = nullptr;
    bad_journal.context.access_journal_capacity = 1;
    check(bad_journal.run() == NBA97_TEXT_ARGUMENT);

    std::array<std::uint8_t, 4> shared{};
    std::array<std::uint8_t, 4> shared_known{1,1,1,1};
    Fixture alias;
    Nba97GameTextRegion alias_regions[2] = {
        {FrameSp + 0x1cu, shared.data(), shared_known.data(), 4},
        {0x800eb678u, shared.data(), shared_known.data(), 4}};
    alias.context.memory = {alias_regions, 2};
    check(alias.run() == NBA97_TEXT_RESOURCE && alias.progress.operations == 2);
    const auto alias_word = static_cast<std::uint32_t>(shared[0]) |
        (static_cast<std::uint32_t>(shared[1]) << 8u) |
        (static_cast<std::uint32_t>(shared[2]) << 16u) |
        (static_cast<std::uint32_t>(shared[3]) << 24u);
    check(alias_word == CallerRa);
}
}

int main() {
    try {
        normal_path_resources_and_order();
        exact_normal_access_sequence();
        alternate_path_and_publications();
        every_live_flag_reload_can_redirect();
        delay_slots_loops_stack_and_unknownness();
        refusal_malformed_and_every_budget_prefix();
        validation_mapping_alignment_alias();
        std::printf("game_scene_resources: %zu checks passed\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s after %zu checks\n", error.what(), checks);
        return 1;
    }
}
