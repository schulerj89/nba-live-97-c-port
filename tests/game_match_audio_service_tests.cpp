#include "recovered/game_match_audio_service.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "match audio service check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t RamSize = 0x110000u;
constexpr std::uint32_t EntrySp = 0x8010ff00u;
constexpr std::uint32_t Frame = EntrySp - 0x20u;
constexpr std::uint32_t Clock = 0x800e430cu;
constexpr std::uint32_t Phase = 0x800170bcu;
constexpr std::uint32_t StreamHandle = 0x8002149cu;
constexpr std::uint32_t AudioResult = 0x80021ee0u;
constexpr std::uint32_t CueValue = 0x80021ee8u;
constexpr std::uint32_t Mode = 0x800fda0cu;
constexpr std::uint32_t Timer = 0x800fda0eu;
constexpr std::uint32_t TimerReset = 0x800fda10u;

struct Seen {
    Nba97GameMatchAudioServiceEvent event{};
    Nba97GameMatchAudioServiceMachine machine{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(RamSize);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(RamSize, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    std::array<Nba97GameMatchAudioServiceAccess, 64> journal{};
    Nba97GameMatchAudioServiceContext context{};
    Nba97GameMatchAudioServiceProgress progress{};
    std::array<std::uint32_t,
        NBA97_GAME_MATCH_AUDIO_SERVICE_CALL_KIND_COUNT> returns{};
    std::vector<Seen> seen;
    unsigned refuse_kind = 0;
    unsigned mutate_kind = 0;
    bool mutate_saved = false;
    bool mutate_all = false;
    bool relocate_frame = false;
    bool partial_clock = false;
    std::uint32_t clock_now = 101;

    explicit Fixture(std::int16_t mode = 0) {
        context.memory = {&region, 1};
        context.operation_budget = 100;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] =
                {0x31000000u + i * 0x01010101u, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8002de64u, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
            {0x11223344u, 15};
        context.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1] =
            {0x55667788u, 15};
        context.machine.hi = {0x89abcdefu, 15};
        context.machine.lo = {0x01234567u, 15};
        returns.fill(1);
        put(Clock, 100, 4);
        put(Mode, static_cast<std::uint16_t>(mode), 2);
        put(Timer, 20, 2);
        put(TimerReset, 40, 2);
        put(Phase, 0, 4);
        put(StreamHandle, 1, 4);
        put(AudioResult, 0x2468ace0u, 4);
        put(CueValue, 0xff80u, 2);
        returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C] = 0;
        returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C] = 1;
        returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008847C] = 1;
        returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80084588] = 2;
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    int run() {
        return nba97_game_match_audio_service(&context, &progress);
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchAudioServiceEvent* event,
        Nba97GameMatchAudioServiceMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        f.seen.push_back({*event, *machine});
        if (event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800A5810) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                {f.clock_now, static_cast<std::uint8_t>(
                    f.partial_clock ? 3u : 15u)};
        } else {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                {f.returns[event->kind], 15};
        }
        if (event->kind == f.mutate_kind) {
            machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1] = {2, 15};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
                {0x800fda20u, 15};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {0xdecafbadu, 15};
            machine->hi = {0x13579bdfu, 15};
            machine->lo = {0x2468ace0u, 15};
        }
        if (f.mutate_all && event->kind == f.mutate_kind) {
            for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
                 ++i)
                machine->registers.gpr[i] =
                    {0x60000000u + i * 0x01010101u, 15};
            machine->hi = {0x55aa55aau, 5};
            machine->lo = {0xaa55aa55u, 10};
        }
        if (f.relocate_frame && event->kind == f.mutate_kind) {
            constexpr std::uint32_t relocated = 0x8010fe00u;
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {relocated, 15};
            f.put(relocated + 0x18u, 0x80005678u, 4);
            f.put(relocated + 0x14u, 0x11112222u, 4);
            f.put(relocated + 0x10u, 0x33334444u, 4);
        }
        if (f.mutate_saved && event->kind == f.mutate_kind) {
            f.put(Frame + 0x18u, 0x80001234u, 4);
            f.put(Frame + 0x14u, 0xabcdef01u, 4);
            f.put(Frame + 0x10u, 0x10203040u, 4);
        }
        return event->kind == f.refuse_kind ? 0 : 1;
    }
};

void quietModesClockAndFrame() {
    const std::array<std::int16_t, 5> modes{{-32768, -1, 0, 4, 32767}};
    for (std::int16_t mode : modes) {
        Fixture f(mode);
        f.clock_now = 0x10u;
        f.put(Clock, 0xfffffff0u, 4);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(f.progress.clock_delta.word == 0x20u &&
            f.progress.clock_delta.known_mask == 15 &&
            f.get(Clock, 4) == 0x10u && f.seen.size() == 1);
        check(f.seen[0].event.pc == 0x8002a270u &&
            f.seen[0].event.delay_slot_pc == 0x8002a274u &&
            f.seen[0].event.entry == 0x800a5810u &&
            f.seen[0].machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].word == 0x8002a278u);
        check(f.get(Frame + 0x18u, 4) == 0x8002de64u &&
            f.get(Frame + 0x14u, 4) == 0x55667788u &&
            f.get(Frame + 0x10u, 4) == 0x11223344u);
        check(f.progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
            f.progress.restored_return_address.word == 0x8002de64u &&
            f.progress.restored_s1.word == 0x55667788u &&
            f.progress.restored_s0.word == 0x11223344u);
    }
}

void modeOneThresholdWrapAndTimeout() {
    struct Case { std::uint32_t phase, timer, old_clock, now, expected;
        bool timeout; };
    const std::array<Case, 9> cases{{
        {0, 479, 100, 101, 478, false},
        {0, 480, 100, 101, 119, false},
        {0x82, 480, 100, 101, 479, false},
        {0, 1, 100, 101, 0, false},
        {0, 0, 100, 101, 0xffff, true},
        {0, 0x7fff, 100, 101, 119, false},
        {0, 0x8000, 100, 101, 0x7fff, false},
        {0, 0xffff, 100, 101, 0xfffe, true},
        {0, 0, 100, 99, 1, false}
    }};
    for (const auto& c : cases) {
        Fixture f(1);
        f.put(Phase, c.phase, 4);
        f.put(Timer, c.timer, 2);
        f.put(Clock, c.old_clock, 4);
        f.clock_now = c.now;
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.get(Timer, 2) == c.expected);
        const auto count = f.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A0A8];
        check(count == (c.timeout ? 1u : 0u));
        if (c.timeout) {
            const Seen& call = f.seen.back();
            check(call.event.pc == 0x8002a400u &&
                call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                    0 &&
                call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                    120 && f.get(Mode, 2) == 2);
        }
    }
}

void modeTwoBranchesAndLiveS0() {
    for (std::uint32_t result : {1u, 0u, 0xffffffffu, 0x80000000u}) {
        Fixture f(2);
        f.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC] = result;
        f.put(0x800fda20u, 0x7777u, 2);
        if (result != 1u) {
            f.mutate_kind = NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC;
        }
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.seen[1].event.pc == 0x8002a424u &&
            f.seen[1].machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_A0].word == 0x2468ace0u);
        check(f.progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_A0].word == 9);
        const bool positive = static_cast<std::int32_t>(result) > 0;
        check(f.progress.call_count[
                NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009DC10] ==
                (positive ? 0u : 1u) &&
            f.progress.call_count[
                NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009F8D8] ==
                (positive ? 0u : 1u));
        if (!positive) {
            check(f.get(0x800fda20u, 2) == 0);
            const Seen& setup = f.seen[f.seen.size() - 2u];
            check(setup.event.pc == 0x8002a43cu &&
                setup.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                    9 &&
                setup.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                    0 &&
                setup.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
                    0);
        }
    }
}

void modeThreeReadinessTree() {
    struct Case { std::uint32_t handle, status, ready1, ready2, ready3;
        bool pump; };
    const std::array<Case, 7> cases{{
        {0, 0, 1, 1, 2, false},
        {1, 0xffffffffu, 1, 1, 2, false},
        {1, 0, 0, 1, 2, true},
        {1, 0, 1, 0, 2, true},
        {1, 0, 1, 1, 0x80000000u, false},
        {1, 0, 1, 1, 1, false},
        {1, 0, 1, 1, 2, true}
    }};
    for (const auto& c : cases) {
        Fixture f(3);
        f.put(StreamHandle, c.handle, 4);
        f.put(Timer, 20, 2);
        f.put(TimerReset, 40, 2);
        f.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C] = c.status;
        f.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C] = c.ready1;
        f.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008847C] = c.ready2;
        f.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80084588] = c.ready3;
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.progress.call_count[
                NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC] ==
                (c.pump ? 1u : 0u));
        check(f.get(Timer, 2) == (c.pump ? 40u : 19u));
    }

    Fixture live_s1(3);
    live_s1.put(StreamHandle, 1, 4);
    live_s1.put(Timer, 20, 2);
    live_s1.mutate_kind =
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C;
    live_s1.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C] =
        0xffffffffu;
    check(live_s1.run() == NBA97_TEXT_COMPLETE &&
        live_s1.get(Timer, 2) == 18 &&
        live_s1.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_T0].word == 0xdecafbadu &&
        live_s1.progress.machine.hi.word == 0x13579bdfu &&
        live_s1.progress.machine.lo.word == 0x2468ace0u);
}

void modeThreeNegativeTimerActions() {
    Fixture cue(3);
    cue.put(StreamHandle, 0, 4);
    cue.put(Timer, 0, 2);
    cue.put(Phase, 0x82, 4);
    check(cue.run() == NBA97_TEXT_COMPLETE &&
        cue.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A46C] == 1);
    const Seen& cue_call = cue.seen.back();
    check(cue_call.event.pc == 0x8002a388u &&
        cue_call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 1 &&
        cue_call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            0xffffff80u);

    Fixture reset(3);
    reset.put(StreamHandle, 0, 4);
    reset.put(Timer, 0, 2);
    reset.put(Phase, 0x81, 4);
    check(reset.run() == NBA97_TEXT_COMPLETE && reset.get(Mode, 2) == 0 &&
        reset.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A46C] == 0);

    Fixture pumped_negative(3);
    pumped_negative.put(TimerReset, 0x8000, 2);
    pumped_negative.put(Phase, 0x82, 4);
    check(pumped_negative.run() == NBA97_TEXT_COMPLETE &&
        pumped_negative.get(Timer, 2) == 0x8000 &&
        pumped_negative.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A46C] == 1);
}

void configureReachable(Fixture& f, unsigned kind) {
    if (kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A0A8) {
        f.put(Mode, 1, 2); f.put(Timer, 0, 2);
    } else if (kind >= NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC) {
        f.put(Mode, 2, 2);
        f.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC] = 0;
    } else if (kind != NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800A5810) {
        f.put(Mode, 3, 2);
        f.put(Phase, 0x82, 4);
        if (kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8002A46C) {
            f.put(StreamHandle, 0, 4); f.put(Timer, 0, 2);
        }
    }
}

void refusalsBudgetsAndSavedMutation() {
    for (unsigned kind = 1;
         kind < NBA97_GAME_MATCH_AUDIO_SERVICE_CALL_KIND_COUNT; ++kind) {
        Fixture f;
        configureReachable(f, kind);
        f.refuse_kind = kind;
        check(f.run() == NBA97_TEXT_IO_REFUSED &&
            f.progress.stopped_entry == f.seen.back().event.entry &&
            f.progress.stopped_pc == f.seen.back().event.pc);
    }

    Fixture complete(3);
    complete.put(Phase, 0x82, 4);
    complete.put(TimerReset, 0x8000, 2);
    check(complete.run() == NBA97_TEXT_COMPLETE);
    const unsigned operations = static_cast<unsigned>(complete.progress.operations);
    for (unsigned budget = 0; budget < operations; ++budget) {
        Fixture f(3);
        f.put(Phase, 0x82, 4);
        f.put(TimerReset, 0x8000, 2);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget && !f.progress.completed);
    }

    Fixture mutated(3);
    mutated.mutate_kind = NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC;
    mutated.mutate_saved = true;
    check(mutated.run() == NBA97_TEXT_COMPLETE &&
        mutated.progress.restored_return_address.word == 0x80001234u &&
        mutated.progress.restored_s1.word == 0xabcdef01u &&
        mutated.progress.restored_s0.word == 0x10203040u &&
        mutated.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp);

    Fixture all_gprs(2);
    all_gprs.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC] = 0;
    all_gprs.mutate_kind =
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8009F8D8;
    all_gprs.mutate_all = true;
    all_gprs.relocate_frame = true;
    check(all_gprs.run() == NBA97_TEXT_COMPLETE &&
        all_gprs.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x8010fe20u &&
        all_gprs.progress.restored_return_address.word == 0x80005678u &&
        all_gprs.progress.restored_s1.word == 0x11112222u &&
        all_gprs.progress.restored_s0.word == 0x33334444u &&
        all_gprs.progress.machine.hi.word == 0x55aa55aau &&
        all_gprs.progress.machine.hi.known_mask == 5 &&
        all_gprs.progress.machine.lo.word == 0xaa55aa55u &&
        all_gprs.progress.machine.lo.known_mask == 10);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (i != NBA97_MATCH_INITIALIZE_SP &&
            i != NBA97_MATCH_INITIALIZE_RA &&
            i != NBA97_MATCH_INITIALIZE_S0 &&
            i != NBA97_GAME_MATCH_CLOCKS_S1)
            check(all_gprs.progress.machine.registers.gpr[i].word ==
                0x60000000u + i * 0x01010101u);
}

void unknownMappingAlignmentAliasAndValidation() {
    Fixture unknown_mode(0);
    unknown_mode.known[unknown_mode.offset(Mode)] = 0;
    check(unknown_mode.run() == NBA97_TEXT_UNKNOWN &&
        unknown_mode.progress.stopped_pc == 0x8002a29cu &&
        unknown_mode.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e);

    Fixture proven_mode(0x0100);
    proven_mode.known[proven_mode.offset(Mode)] = 0;
    check(proven_mode.run() == NBA97_TEXT_COMPLETE);

    Fixture unknown_store(0);
    unknown_store.partial_clock = true;
    check(unknown_store.run() == NBA97_TEXT_COMPLETE &&
        unknown_store.known[unknown_store.offset(Clock) + 2u] == 0 &&
        unknown_store.known[unknown_store.offset(Clock) + 3u] == 0);

    Fixture no_bitmap(0);
    no_bitmap.region.known = nullptr;
    no_bitmap.partial_clock = true;
    check(no_bitmap.run() == NBA97_TEXT_ARGUMENT &&
        no_bitmap.progress.stopped_pc == 0x8002a290u);

    Fixture unknown_sp;
    unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
        .known_mask = 14;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8002a268u &&
        unknown_sp.progress.operations == 0);

    Fixture unaligned;
    unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8002a268u);

    Fixture missing;
    missing.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x90000020u, 15};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_address == 0x90000018u);

    Fixture unknown_ra;
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 7;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8002a45cu);

    Fixture alias(0);
    alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Mode + 0x10u, 15};
    alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {2, 15};
    alias.returns[NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC] = 1;
    check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800AD9FC] == 1);

    std::array<std::uint8_t, 0x20> wrap_bytes{};
    std::array<std::uint8_t, 0x20> wrap_known{};
    wrap_known.fill(1);
    Fixture wrapped;
    Nba97GameTextRegion regions[2] = {
        wrapped.region,
        {0xffffffe0u, wrap_bytes.data(), wrap_known.data(), wrap_bytes.size()}
    };
    wrapped.context.memory = {regions, 2};
    wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0, 15};
    check(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xffffffe0u &&
        wrapped.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0);

    Fixture overlap;
    Nba97GameTextRegion duplicates[2] = {overlap.region, overlap.region};
    overlap.context.memory = {duplicates, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_machine;
    bad_machine.context.machine.hi.known_mask = 16;
    check(bad_machine.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameMatchAudioServiceContext empty{};
    Nba97GameMatchAudioServiceProgress progress{};
    check(nba97_game_match_audio_service(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_match_audio_service(&empty, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    quietModesClockAndFrame();
    modeOneThresholdWrapAndTimeout();
    modeTwoBranchesAndLiveS0();
    modeThreeReadinessTree();
    modeThreeNegativeTimerActions();
    refusalsBudgetsAndSavedMutation();
    unknownMappingAlignmentAliasAndValidation();
    std::printf("%u game match audio service checks passed\n", checks);
    return 0;
}
