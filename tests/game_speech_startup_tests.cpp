#include "game_speech_startup.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game speech startup check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffc00u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t FrameSp = EntrySp - 0x20u;
constexpr std::uint32_t CallerRa = 0x800802bcu;

struct Call {
    Nba97GameSpeechStartupEvent event{};
    Nba97GameSpeechStartupRegisters before{};
    Nba97GameSpeechStartupRegisters after{};
};

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x400> stack{};
    std::array<std::uint8_t, 0x400> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    std::array<Nba97GameSpeechStartupAccess, 64> journal{};
    Nba97GameSpeechStartupContext context{};
    Nba97GameSpeechStartupProgress progress{};
    std::vector<Call> calls;
    std::vector<Nba97GameSpeechStartupWord> ready{{1, 0x0f}};
    std::vector<Nba97GameSpeechStartupWord> clocks{{100, 0x0f}};
    std::size_t ready_index = 0;
    std::size_t clock_index = 0;
    std::uint32_t handle = 0x81234560u;
    std::uint8_t handle_known = 0x0f;
    std::uint32_t voice = 0x8abcdef0u;
    std::uint8_t voice_known = 0x0f;
    std::size_t refuse_ordinal = static_cast<std::size_t>(-1);
    std::size_t malformed_ordinal = static_cast<std::size_t>(-1);
    bool mutate_handle_after_open = false;
    bool mutate_live_s0_at_ready = false;
    bool mutate_live_s0_at_poll_clock = false;
    bool alias_fifth_argument_with_handle = false;
    bool relocate_on_cleanup = false;
    bool preserve_marker = false;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put(0x80015018u, 0);
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 1000;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x41000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {
            0x11223344u, 0x06};
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }

    std::uint8_t* data(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.known ? region.known + (address - region.base) :
                    nullptr;
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value,
        std::uint8_t mask = 0x0f) {
        auto* bytes = data(address);
        auto* marks = known(address);
        for (unsigned i = 0; i < 4; ++i) {
            bytes[i] = static_cast<std::uint8_t>(value >> (8u * i));
            if (marks)
                marks[i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const std::uint8_t* bytes = nullptr;
        for (const auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) + 4u <= region.size)
                bytes = region.data + (address - region.base);
        if (!bytes)
            std::abort();
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[i]) << (8u * i);
        return value;
    }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameSpeechStartupEvent* event,
        Nba97GameSpeechStartupRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        Call call{*event, *registers, {}};
        const std::size_t ordinal = f.calls.size();
        if (ordinal == f.refuse_ordinal) {
            call.after = *registers;
            f.calls.push_back(call);
            return 0;
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800853F4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.handle, f.handle_known};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800859C8 &&
            f.mutate_handle_after_open)
            f.put(0x8002149cu, 0x8badf00du, 0x0f);
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800889F4 &&
            f.alias_fifth_argument_with_handle)
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                0x8002148cu, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80029CA0 &&
            f.alias_fifth_argument_with_handle)
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {FrameSp, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80083D38)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.voice, f.voice_known};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810) {
            const auto index = f.clock_index < f.clocks.size() ?
                f.clock_index++ : f.clocks.size() - 1u;
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = f.clocks[index];
            if (f.mutate_live_s0_at_poll_clock && f.clock_index > 1u)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {99, 0x0f};
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C) {
            const auto index = f.ready_index < f.ready.size() ?
                f.ready_index++ : f.ready.size() - 1u;
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = f.ready[index];
            if (f.mutate_live_s0_at_ready)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {99, 0x0f};
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8002ABB4 &&
            f.relocate_on_cleanup) {
            constexpr std::uint32_t NewFrame = Stack + 0x40u;
            f.put(NewFrame + 0x1cu, 0x81223344u, 0x0f);
            f.put(NewFrame + 0x18u, 0xa1b2c3d4u, 0x05);
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {NewFrame, 0x0f};
        }
        if (f.preserve_marker)
            registers->gpr[NBA97_MATCH_INITIALIZE_T8] = {0x13579bdfu, 0x0a};
        if (ordinal == f.malformed_ordinal)
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
        call.after = *registers;
        f.calls.push_back(call);
        return 1;
    }

    int run() { return nba97_game_speech_startup(&context, &progress); }
};

bool same_word(Nba97GameSpeechStartupWord a,
    Nba97GameSpeechStartupWord b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}
bool same_registers(const Nba97GameSpeechStartupRegisters& a,
    const Nba97GameSpeechStartupRegisters& b) {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (!same_word(a.gpr[i], b.gpr[i]))
            return false;
    return true;
}

void exact_languages_calls_arguments_and_fifth_stack_word() {
    const std::array<std::uint32_t, 3> language{{1, 2, 7}};
    const std::array<std::uint32_t, 3> name{{
        0x80027bb0u, 0x80027bc0u, 0x80027bd0u}};
    for (unsigned route = 0; route < language.size(); ++route) {
        Fixture f;
        f.put(0x80015018u, language[route]);
        f.mutate_handle_after_open = true;
        f.preserve_marker = true;
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(f.calls.size() == 11 && f.progress.callbacks_completed == 11 &&
            f.progress.operations == (route == 2 ? 23u : 22u));
        const std::array<std::uint32_t, 11> pcs{{
            0x80080114u, 0x80080124u, 0x8008018cu, 0x8008019cu,
            0x800801bcu, 0x800801c8u, 0x800801dcu, 0x800801e4u,
            0x800801ecu, 0x800801f8u, 0x8008022cu}};
        for (unsigned i = 0; i < pcs.size(); ++i) {
            check(f.calls[i].event.pc == pcs[i] &&
                f.calls[i].event.delay_slot_pc == pcs[i] + 4u &&
                f.calls[i].before.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                    pcs[i] + 8u);
        }
        check(f.calls[1].before.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x6000u &&
            f.calls[1].before.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x2000u &&
            f.calls[1].before.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0x20u);
        check(f.calls[2].before.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.handle &&
            f.calls[2].before.gpr[NBA97_MATCH_INITIALIZE_A1].word == name[route]);
        check(f.calls[3].before.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x3cu &&
            f.calls[3].before.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x400u &&
            f.calls[3].before.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0);
        check(f.calls[4].before.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                0x8badf00du &&
            f.calls[4].before.gpr[NBA97_MATCH_INITIALIZE_A1].word == 5u &&
            f.calls[4].before.gpr[NBA97_MATCH_INITIALIZE_A2].word == 10000u &&
            f.calls[4].before.gpr[NBA97_MATCH_INITIALIZE_A3].word == 0x6000u &&
            f.calls[4].event.argument_count == 5 &&
            f.get(FrameSp + 0x10u) == 1u);
        check(f.calls[5].before.gpr[NBA97_MATCH_INITIALIZE_A0].word == 15u &&
            f.calls[5].before.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                0xffffffffu);
        check(f.calls[6].before.gpr[NBA97_MATCH_INITIALIZE_A0].word == f.voice &&
            f.calls[6].before.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0);
        check(f.calls.back().before.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0 &&
            f.calls.back().before.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0);
        check(f.get(0x80103fb0u) == 0 && f.get(0x800c4568u) == 0 &&
            f.get(0x8002149cu) == 0x8badf00du &&
            f.get(0x800dc7e8u) == f.voice);
        check(f.progress.language.word == language[route] &&
            f.progress.speech_handle.word == f.handle &&
            f.progress.published_voice.word == f.voice &&
            f.progress.deadline.word == 340u);
        check(f.progress.restored_return_address.word == CallerRa &&
            f.progress.restored_s0.word == 0x11223344u &&
            f.progress.restored_s0.known_mask == 0x06 &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T8].word ==
                0x13579bdfu &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T8].known_mask ==
                0x0a);
    }
}

void full_gpr_first_call_and_access_order() {
    Fixture f;
    const auto entry = f.context.registers;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.frame_stack_pointer == FrameSp);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        auto expected = entry.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_SP)
            expected = {FrameSp, 0x0f};
        if (i == NBA97_MATCH_INITIALIZE_RA)
            expected = {0x8008011cu, 0x0f};
        if (i == NBA97_MATCH_INITIALIZE_AT)
            expected = {0x800c0000u, 0x0f};
        check(same_word(f.calls[0].before.gpr[i], expected));
    }
    const std::array<std::uint32_t, 11> pc{{
        0x800800fcu, 0x80080100u, 0x80080108u, 0x80080110u,
        0x80080130u, 0x80080140u, 0x80080180u, 0x800801a8u,
        0x800801c0u, 0x800801d8u, 0x80080234u}};
    for (unsigned i = 0; i < pc.size(); ++i)
        check(f.journal[i].pc == pc[i]);
    check(f.journal[10].pc == 0x80080234u &&
        f.journal[11].pc == 0x80080238u &&
        f.progress.access_events == 12u);
}

void ready_later_signed_timeout_equality_overflow_and_negative() {
    Fixture later;
    later.ready = {{0, 0x0f}, {1, 0x0f}};
    later.clocks = {{100, 0x0f}, {101, 0x0f}};
    check(later.run() == NBA97_TEXT_COMPLETE &&
        later.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C] == 2 &&
        later.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC] == 2);

    Fixture equality;
    equality.ready = {{0, 0x0f}};
    equality.clocks = {{100, 0x0f}, {340, 0x0f}, {341, 0x0f}};
    check(equality.run() == NBA97_TEXT_COMPLETE &&
        equality.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C] == 2 &&
        equality.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC] == 2 &&
        equality.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810] == 3);

    Fixture overflow;
    overflow.ready = {{0, 0x0f}};
    overflow.clocks = {{0x7fffff80u, 0x0f}, {0x7fffffffu, 0x0f}};
    check(overflow.run() == NBA97_TEXT_COMPLETE &&
        overflow.progress.deadline.word == 0x80000070u &&
        overflow.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C] == 1);

    Fixture negative;
    negative.ready = {{0, 0x0f}};
    negative.clocks = {{0xffffff00u, 0x0f}, {0xffffffefu, 0x0f},
        {0xfffffff1u, 0x0f}};
    check(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.progress.deadline.word == 0xfffffff0u &&
        negative.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C] == 2);

    Fixture live_s0;
    live_s0.ready = {{0, 0x0f}};
    live_s0.clocks = {{100, 0x0f}, {100, 0x0f}};
    live_s0.mutate_live_s0_at_ready = true;
    check(live_s0.run() == NBA97_TEXT_COMPLETE &&
        live_s0.progress.deadline.word == 340u &&
        live_s0.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C] == 1);

    Fixture clock_live_s0;
    clock_live_s0.ready = {{0, 0x0f}};
    clock_live_s0.clocks = {{100, 0x0f}, {100, 0x0f}};
    clock_live_s0.mutate_live_s0_at_poll_clock = true;
    check(clock_live_s0.run() == NBA97_TEXT_COMPLETE &&
        clock_live_s0.progress.deadline.word == 340u &&
        clock_live_s0.progress.call_count[
            NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C] == 1);
}

void unknown_delays_knownness_and_child_mutable_stack() {
    Fixture language;
    language.put(0x80015018u, 1, 0x0e);
    check(language.run() == NBA97_TEXT_UNKNOWN &&
        language.progress.stopped_pc == 0x80080144u &&
        language.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 2 &&
        language.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0f);

    Fixture second_language;
    second_language.put(0x80015018u, 2, 0x01);
    check(second_language.run() == NBA97_TEXT_UNKNOWN &&
        second_language.progress.stopped_pc == 0x8008014cu);

    Fixture ready_unknown;
    ready_unknown.ready = {{0, 0x0e}};
    check(ready_unknown.run() == NBA97_TEXT_UNKNOWN &&
        ready_unknown.progress.stopped_pc == 0x80080200u &&
        ready_unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0 &&
        ready_unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
            0x0f);

    Fixture ready_known_nonzero;
    ready_known_nonzero.ready = {{0x100u, 0x02}};
    check(ready_known_nonzero.run() == NBA97_TEXT_COMPLETE);

    Fixture clock_unknown;
    clock_unknown.ready = {{0, 0x0f}};
    clock_unknown.clocks = {{100, 0x0f}, {0, 0x07}};
    auto expected_unknown_clock = clock_unknown.context.registers;
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_AT] = {
        0x800e0000u, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0e};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_V1] = {0, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_A0] = {0, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_A1] = {0, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_A2] = {
        0x2710u, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_A3] = {0x6000u, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_S0] = {340, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_SP] = {FrameSp, 0x0f};
    expected_unknown_clock.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x80080210u, 0x0f};
    check(clock_unknown.run() == NBA97_TEXT_UNKNOWN &&
        clock_unknown.progress.stopped_pc == 0x80080214u &&
        clock_unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0 &&
        clock_unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e &&
        same_registers(clock_unknown.progress.registers,
            expected_unknown_clock));

    Fixture partial_handle;
    partial_handle.handle_known = 0x05;
    partial_handle.voice_known = 0x0a;
    check(partial_handle.run() == NBA97_TEXT_COMPLETE &&
        partial_handle.progress.speech_handle.known_mask == 0x05 &&
        partial_handle.progress.published_voice.known_mask == 0x0a);
    for (unsigned i = 0; i < 4; ++i) {
        check(partial_handle.ram_known[0x2149cu + i] == ((0x05u >> i) & 1u));
        check(partial_handle.ram_known[0xdc7e8u + i] == ((0x0au >> i) & 1u));
    }

    Fixture relocated;
    relocated.relocate_on_cleanup = true;
    check(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.restored_return_address.word == 0x81223344u &&
        relocated.progress.restored_s0.word == 0xa1b2c3d4u &&
        relocated.progress.restored_s0.known_mask == 0x05 &&
        relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Stack + 0x60u);

    Fixture unknown_ra;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x07};
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x80080240u &&
        unknown_ra.progress.restored_return_address.known_mask == 0x07 &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);
}

void refusal_bounded_runaway_mapping_and_all_budget_prefixes() {
    Fixture baseline;
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    const auto complete_operations = baseline.progress.operations;
    for (std::size_t budget = 0; budget < complete_operations; ++budget) {
        Fixture first;
        Fixture repeat;
        first.context.operation_budget = budget;
        repeat.context.operation_budget = budget;
        check(first.run() == NBA97_TEXT_LIMIT && repeat.run() == NBA97_TEXT_LIMIT &&
            first.progress.operations == budget &&
            same_registers(first.progress.registers, repeat.progress.registers) &&
            first.progress.stopped_pc == repeat.progress.stopped_pc &&
            first.progress.stopped_address == repeat.progress.stopped_address &&
            first.progress.stopped_entry == repeat.progress.stopped_entry &&
            first.progress.callbacks_completed == repeat.progress.callbacks_completed &&
            first.progress.access_events == repeat.progress.access_events);
        for (std::size_t i = 0; i < first.progress.access_events; ++i)
            check(first.journal[i].pc == repeat.journal[i].pc &&
                first.journal[i].address == repeat.journal[i].address &&
                first.journal[i].value == repeat.journal[i].value &&
                first.journal[i].known_mask == repeat.journal[i].known_mask);
    }
    Fixture exact;
    exact.context.operation_budget = complete_operations;
    check(exact.run() == NBA97_TEXT_COMPLETE &&
        exact.progress.operations == complete_operations);

    for (std::size_t ordinal = 0; ordinal < 11; ++ordinal) {
        Fixture refused;
        refused.refuse_ordinal = ordinal;
        check(refused.run() == NBA97_TEXT_IO_REFUSED &&
            refused.calls.size() == ordinal + 1u &&
            refused.progress.callbacks_completed == ordinal &&
            refused.progress.stopped_pc == refused.calls.back().event.pc &&
            refused.progress.stopped_entry == refused.calls.back().event.entry);
        Fixture malformed;
        malformed.malformed_ordinal = ordinal;
        check(malformed.run() == NBA97_TEXT_ARGUMENT &&
            malformed.progress.callbacks_completed == ordinal &&
            malformed.progress.stopped_pc == malformed.calls.back().event.pc);
    }

    Fixture no_io;
    no_io.context.io = nullptr;
    check(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations == 5 && no_io.progress.stores == 4 &&
        no_io.progress.stopped_pc == 0x80080114u);

    Fixture runaway;
    runaway.ready = {{0, 0x0f}};
    runaway.clocks = {{0, 0x0f}};
    runaway.context.operation_budget = 40;
    check(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 40 && !runaway.progress.completed &&
        runaway.progress.call_count[NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC] > 1);

    Fixture unaligned;
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2u;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 1 &&
        unaligned.progress.stopped_pc == 0x800800fcu);
    Fixture missing;
    missing.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        0x90000020u, 0x0f};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x800800fcu);
    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x800800fcu &&
        unknown_sp.progress.frame_stack_pointer == FrameSp);
    Fixture malformed_memory;
    *malformed_memory.known(FrameSp + 0x1cu) = 2;
    check(malformed_memory.run() == NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc == 0x800800fcu);
    Fixture overlap;
    Nba97GameTextRegion duplicate[2] = {overlap.regions[0], overlap.regions[0]};
    overlap.context.memory = {duplicate, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture wrapped;
    wrapped.regions[1].base = 0xfffffffcu;
    wrapped.regions[1].size = 8;
    check(wrapped.run() == NBA97_TEXT_ARGUMENT);
    Fixture alias;
    alias.alias_fifth_argument_with_handle = true;
    check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.calls[4].before.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            alias.handle && alias.get(0x8002149cu) == 1u &&
        alias.progress.restored_return_address.word == CallerRa);
    Fixture no_masks;
    no_masks.context.registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask = 0x0f;
    no_masks.regions[0].known = nullptr;
    no_masks.regions[1].known = nullptr;
    check(no_masks.run() == NBA97_TEXT_COMPLETE);
    Fixture no_masks_partial;
    no_masks_partial.regions[0].known = nullptr;
    no_masks_partial.regions[1].known = nullptr;
    no_masks_partial.context.registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask =
        0x0f;
    no_masks_partial.handle_known = 0x07;
    check(no_masks_partial.run() == NBA97_TEXT_ARGUMENT &&
        no_masks_partial.progress.stopped_pc == 0x80080140u);
    Fixture bad_mask;
    bad_mask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameSpeechStartupContext context{};
    Nba97GameSpeechStartupProgress progress{};
    check(nba97_game_speech_startup(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_speech_startup(&context, nullptr) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_languages_calls_arguments_and_fifth_stack_word();
    full_gpr_first_call_and_access_order();
    ready_later_signed_timeout_equality_overflow_and_negative();
    unknown_delays_knownness_and_child_mutable_stack();
    refusal_bounded_runaway_mapping_and_all_budget_prefixes();
    std::printf("%u game speech startup checks passed\n", checks);
    return 0;
}
