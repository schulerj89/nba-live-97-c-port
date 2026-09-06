#include "game_audio_stream_pump.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game audio stream pump check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807ff800u;
constexpr std::uint32_t FrameSp = EntrySp - 0x20u;
constexpr std::uint32_t CallerRa = 0x800801ecu;

struct Call {
    Nba97GameAudioStreamPumpEvent event{};
    Nba97GameAudioStreamPumpRegisters before{};
};

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0xc5000);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0xc5000, 1);
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    std::array<Nba97GameAudioStreamPumpAccess, 128> journal{};
    Nba97GameAudioStreamPumpContext context{};
    Nba97GameAudioStreamPumpProgress progress{};
    std::vector<Call> calls;
    std::vector<Nba97GameAudioStreamPumpWord> statuses{{0, 0x0f}};
    Nba97GameAudioStreamPumpWord gate{0, 0x0f};
    Nba97GameAudioStreamPumpWord handler_return{0xfeed1234u, 0x0f};
    unsigned status_index = 0;
    unsigned refuse_kind = 0;
    unsigned malformed_kind = 0;
    bool relocate_in_handler = false;
    bool mutate_saved_ra_in_query = false;
    bool mutate_return_in_query = false;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put8(0x800c43b0u, 5);
        put32(0x800c438cu, 0x81234560u);
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 256;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x11000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_FP] = {0xa1b2c3d4u, 0x06};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }

    std::uint8_t* bytes(std::uint32_t address) {
        return address >= Stack ? stack.data() + (address - Stack) :
            ram.data() + (address - Ram);
    }
    std::uint8_t* known(std::uint32_t address) {
        return address >= Stack ? stack_known.data() + (address - Stack) :
            ram_known.data() + (address - Ram);
    }
    void put8(std::uint32_t address, std::uint8_t value,
        std::uint8_t is_known = 1) {
        *bytes(address) = value;
        *known(address) = is_known;
    }
    void put32(std::uint32_t address, std::uint32_t value,
        std::uint8_t mask = 0x0f) {
        for (unsigned i = 0; i < 4; ++i) {
            bytes(address)[i] = static_cast<std::uint8_t>(value >> (8u * i));
            known(address)[i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get32(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes(address)[i]) << (8u * i);
        return value;
    }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioStreamPumpEvent* event,
        Nba97GameAudioStreamPumpRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *registers});
        if (event->kind == f.refuse_kind)
            return 0;
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = f.gate;
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018) {
            const auto index = f.status_index < f.statuses.size() ?
                f.status_index : static_cast<unsigned>(f.statuses.size() - 1u);
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = f.statuses[index];
            ++f.status_index;
            if (f.mutate_saved_ra_in_query)
                f.put32(FrameSp + 0x1cu, 0x81223344u, 0x07);
            if (f.mutate_return_in_query)
                f.put32(FrameSp + 0x14u, 0x89abcdefu, 0x05);
        }
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = f.handler_return;
            if (f.relocate_in_handler) {
                constexpr std::uint32_t relocated = Stack + 0x300u;
                f.put32(relocated + 0x10u, 0);
                f.put32(relocated + 0x18u, 0x55667788u, 0x05);
                f.put32(relocated + 0x1cu, 0x81224488u);
                registers->gpr[NBA97_MATCH_INITIALIZE_FP] = {relocated, 0x0f};
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {0xdead0000u, 0x0f};
            }
        }
        if (event->kind == f.malformed_kind)
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
        return 1;
    }

    int run() {
        return nba97_game_audio_stream_pump(&context, &progress);
    }
};

void initial_gate_all_flags_and_exact_dispatch() {
    Fixture negative;
    negative.gate = {0xffffffffu, 0x0f};
    check(negative.run() == NBA97_TEXT_COMPLETE && negative.progress.completed &&
        negative.progress.returned_value.word == 0 && negative.calls.size() == 1 &&
        negative.progress.operations == 6);
    Fixture positive;
    positive.gate = {1, 0x0f};
    check(positive.run() == NBA97_TEXT_COMPLETE && positive.progress.completed &&
        positive.progress.initial_status.word == 1 &&
        positive.progress.call_count[
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018] == 1);

    for (unsigned flag = 0; flag < 256; ++flag) {
        Fixture f;
        f.put8(0x800c43b0u, static_cast<std::uint8_t>(flag));
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        const unsigned mode = flag & 5u;
        const unsigned expected_queries = mode == 5u ||
            (mode == 4u && (flag & 2u)) ? 1u : 0u;
        check(f.progress.call_count[
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018] == expected_queries);
        check(f.progress.call_count[
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190] == expected_queries);
        check(f.progress.call_count[
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088288] == 0);
        check(f.progress.first_flags.word == flag &&
            f.progress.first_flags.known_mask == 0x0f);
    }

    Fixture prefix;
    const auto entry = prefix.context.registers;
    check(prefix.run() == NBA97_TEXT_COMPLETE);
    check(prefix.progress.stopped_pc == 0 &&
        prefix.progress.stopped_address == 0 &&
        prefix.progress.stopped_entry == 0);
    check(prefix.calls.front().event.pc == 0x80083f00u &&
        prefix.calls.front().event.delay_slot_pc == 0x80083f04u &&
        prefix.calls.front().event.entry == 0x8008472cu);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        auto expected = entry.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_SP || i == NBA97_MATCH_INITIALIZE_FP)
            expected = {FrameSp, 0x0f};
        if (i == NBA97_MATCH_INITIALIZE_RA)
            expected = {0x80083f08u, 0x0f};
        check(prefix.calls.front().before.gpr[i].word == expected.word &&
            prefix.calls.front().before.gpr[i].known_mask == expected.known_mask);
    }
    check(prefix.journal[0].pc == 0x80083ef0u &&
        prefix.journal[1].pc == 0x80083ef4u &&
        prefix.journal[2].pc == 0x80083efcu &&
        prefix.journal[3].pc == 0x80083f20u);
}

void mode5_signed_statuses_loops_and_live_frames() {
    const std::array<std::uint32_t, 7> values{{
        0xfffffff6u, 0xfffffff7u, 0xffffffffu, 0u, 1u,
        0x80000000u, 0x7fffffffu}};
    for (auto value : values) {
        Fixture f;
        const bool repeats = value == 0xfffffff6u || value == 1u ||
            value == 0x80000000u || value == 0x7fffffffu;
        f.statuses = repeats ?
            std::vector<Nba97GameAudioStreamPumpWord>{{value, 0x0f}, {0, 0x0f}} :
            std::vector<Nba97GameAudioStreamPumpWord>{{value, 0x0f}};
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.progress.call_count[
                NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018] ==
                (repeats ? 2u : 1u) &&
            f.progress.call_count[
                NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0] ==
                (repeats ? 1u : 0u) &&
            f.progress.returned_value.word == 0);
    }

    Fixture relocated;
    relocated.statuses = {{1, 0x0f}};
    relocated.relocate_in_handler = true;
    check(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.call_count[
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0] == 1 &&
        relocated.progress.returned_value.word == relocated.handler_return.word &&
        relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Stack + 0x320u &&
        relocated.progress.restored_return_address.word == 0x81224488u &&
        relocated.progress.restored_s8.word == 0x55667788u &&
        relocated.progress.restored_s8.known_mask == 0x05);
}

void mode4_contradictory_handler_and_live_return_slot() {
    const std::array<std::uint32_t, 7> values{{
        0xfffffff6u, 0xfffffff7u, 0xffffffffu, 0u, 1u,
        0x80000000u, 0x7fffffffu}};
    for (auto value : values) {
        Fixture f;
        f.put8(0x800c43b0u, 6);
        const bool repeats = value == 0xfffffff6u || value == 1u ||
            value == 0x80000000u || value == 0x7fffffffu;
        f.statuses = repeats ?
            std::vector<Nba97GameAudioStreamPumpWord>{{value, 0x0f}, {0, 0x0f}} :
            std::vector<Nba97GameAudioStreamPumpWord>{{value, 0x0f}};
        f.mutate_return_in_query = true;
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.progress.call_count[
                NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018] ==
                (repeats ? 2u : 1u) &&
            f.progress.call_count[
                NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088288] == 0 &&
            f.progress.returned_value.word == 0x89abcdefu &&
            f.progress.returned_value.known_mask == 0x05);
    }
}

void knownness_stack_mapping_refusal_and_every_budget_prefix() {
    Fixture unknown_gate;
    unknown_gate.gate = {0, 0x07};
    check(unknown_gate.run() == NBA97_TEXT_UNKNOWN &&
        unknown_gate.progress.stopped_pc == 0x80083f08u);

    Fixture unknown_flags;
    unknown_flags.put8(0x800c43b0u, 0, 0);
    check(unknown_flags.run() == NBA97_TEXT_UNKNOWN &&
        unknown_flags.progress.stopped_pc == 0x80083f34u &&
        unknown_flags.progress.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e &&
        unknown_flags.progress.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].known_mask == 0x0f);

    Fixture slti_unknown;
    slti_unknown.statuses = {{0xffffffffu, 0x08}};
    check(slti_unknown.run() == NBA97_TEXT_UNKNOWN &&
        slti_unknown.progress.stopped_pc == 0x80083fb0u &&
        slti_unknown.progress.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].known_mask == 0x0e);

    Fixture mutable_ra;
    mutable_ra.mutate_saved_ra_in_query = true;
    check(mutable_ra.run() == NBA97_TEXT_UNKNOWN &&
        mutable_ra.progress.stopped_pc == 0x800840e8u &&
        mutable_ra.progress.restored_return_address.word == 0x81223344u &&
        mutable_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture baseline;
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    const auto operation_count = baseline.progress.operations;
    for (std::size_t budget = 0; budget < operation_count; ++budget) {
        Fixture first;
        Fixture repeat;
        first.context.operation_budget = budget;
        repeat.context.operation_budget = budget;
        check(first.run() == NBA97_TEXT_LIMIT &&
            repeat.run() == NBA97_TEXT_LIMIT &&
            first.progress.operations == budget &&
            first.progress.stopped_pc == repeat.progress.stopped_pc &&
            first.progress.stopped_address == repeat.progress.stopped_address &&
            first.progress.stopped_entry == repeat.progress.stopped_entry);
    }

    const std::array<unsigned, 4> reachable{{
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C,
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190,
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018,
        NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0}};
    for (auto kind : reachable) {
        Fixture refused;
        if (kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0)
            refused.statuses = {{1, 0x0f}, {0, 0x0f}};
        refused.refuse_kind = kind;
        check(refused.run() == NBA97_TEXT_IO_REFUSED &&
            refused.progress.stopped_entry != 0);
        Fixture malformed;
        if (kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0)
            malformed.statuses = {{1, 0x0f}, {0, 0x0f}};
        malformed.malformed_kind = kind;
        check(malformed.run() == NBA97_TEXT_ARGUMENT);
    }

    Fixture runaway;
    runaway.statuses = {{1, 0x0f}};
    runaway.context.operation_budget = 40;
    check(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 40 &&
        runaway.progress.call_count[
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018] > 1);

    Fixture unaligned;
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80083ef0u);
    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x80083ef0u);
    Fixture missing;
    missing.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        0x90000020u, 0x0f};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x80083ef0u);
    Fixture overlap;
    Nba97GameTextRegion duplicate[2] = {overlap.regions[0], overlap.regions[0]};
    overlap.context.memory = {duplicate, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture bad_mask;
    bad_mask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameAudioStreamPumpContext empty{};
    Nba97GameAudioStreamPumpProgress progress{};
    check(nba97_game_audio_stream_pump(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_pump(&empty, nullptr) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    initial_gate_all_flags_and_exact_dispatch();
    mode5_signed_statuses_loops_and_live_frames();
    mode4_contradictory_handler_and_live_return_slot();
    knownness_stack_mapping_refusal_and_every_budget_prefix();
    std::printf("%u game audio stream pump checks passed\n", checks);
    return 0;
}
