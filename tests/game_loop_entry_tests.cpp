#include "recovered/game_loop_entry.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game loop-entry check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fff80u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;

struct Fixture {
    std::array<std::uint8_t, 0x100> stack{}, known{};
    Nba97GameTextRegion region{Stack, stack.data(), known.data(), stack.size()};
    std::array<Nba97GameLoopEntryAccess, 2> journal{};
    Nba97GameLoopEntryContext context{};
    Nba97GameLoopEntryProgress progress{};
    std::vector<Nba97GameLoopEntryEvent> calls;
    bool refuse = false;
    bool malformed = false;
    bool relocate = false;
    bool unknown_return = false;
    bool observed_save = false;

    Fixture() {
        stack.fill(0xcd);
        known.fill(1);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x10000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x11223344u, 0x0f};
        context.memory = {&region, 1};
        context.operation_budget = 3;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - region.base);
    }
    void put(std::uint32_t address, std::uint32_t value,
        std::uint8_t mask = 0x0f) {
        const auto at = offset(address);
        for (unsigned i = 0; i < 4; ++i) {
            region.data[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            if (region.known)
                region.known[at + i] = static_cast<std::uint8_t>(
                    (mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(region.data[at + i]) << (i * 8u);
        return value;
    }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameLoopEntryEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        f.observed_save = f.get(FrameSp + 0x10u) == 0x11223344u &&
            registers->gpr[NBA97_MATCH_INITIALIZE_SP].word == FrameSp &&
            registers->gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8002dc48u;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 0x0f};
        if (f.relocate) {
            const std::uint32_t relocated = FrameSp - 0x20u;
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {relocated, 0x0f};
            f.put(relocated + 0x10u, 0xa1b2c3d4u);
        } else if (f.unknown_return) {
            f.put(FrameSp + 0x10u, 0, 0x03);
        } else {
            f.put(FrameSp + 0x10u, 0x55667788u);
        }
        if (f.malformed)
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
        return f.refuse ? 0 : 1;
    }
    int run() { return nba97_game_loop_entry(&context, &progress); }
};

void exact_call_and_return() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.observed_save && f.calls.size() == 1);
    check(f.calls[0].pc == 0x8002dc40u &&
        f.calls[0].delay_slot_pc == 0x8002dc44u &&
        f.calls[0].entry == 0x80068bf8u &&
        f.calls[0].operation == 2 &&
        f.calls[0].kind == NBA97_GAME_LOOP_ENTRY_MATCH_TICK &&
        f.calls[0].argument_count == 0);
    check(f.progress.operations == 3 && f.progress.accesses == 2 &&
        f.progress.stores == 1 && f.progress.reads == 1 &&
        f.progress.callbacks_completed == 1 &&
        f.progress.access_events == 2);
    check(f.progress.frame_stack_pointer == FrameSp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.restored_return_address.word == 0x55667788u &&
        f.progress.restored_return_address.known_mask == 0x0f &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x55667788u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xcafebabeu);
    check(f.journal[0].kind == NBA97_GAME_LOOP_ENTRY_STORE &&
        f.journal[0].pc == 0x8002dc3cu &&
        f.journal[0].address == EntrySp - 8u &&
        f.journal[0].value == 0x11223344u &&
        f.journal[0].operation == 1 &&
        f.journal[1].kind == NBA97_GAME_LOOP_ENTRY_READ &&
        f.journal[1].pc == 0x8002dc48u &&
        f.journal[1].address == EntrySp - 8u &&
        f.journal[1].value == 0x55667788u &&
        f.journal[1].operation == 3);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);
    for(unsigned i=0;i<32;++i) {
        if(i==NBA97_MATCH_INITIALIZE_V0 || i==NBA97_MATCH_INITIALIZE_SP ||
           i==NBA97_MATCH_INITIALIZE_RA)continue;
        check(f.progress.registers.gpr[i].word==f.context.registers.gpr[i].word &&
              f.progress.registers.gpr[i].known_mask==f.context.registers.gpr[i].known_mask);
    }
}

void live_stack_and_unknown_return() {
    Fixture relocated;
    relocated.relocate = true;
    check(relocated.run() == NBA97_TEXT_COMPLETE && relocated.progress.completed);
    check(relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        FrameSp - 0x20u + 0x18u);
    check(relocated.progress.restored_return_address.word == 0xa1b2c3d4u &&
        relocated.journal[1].address == FrameSp - 0x10u &&
        relocated.get(FrameSp + 0x10u) == 0x11223344u);

    Fixture unknown;
    unknown.unknown_return = true;
    check(unknown.run() == NBA97_TEXT_UNKNOWN && !unknown.progress.completed);
    check(unknown.progress.operations == 3 && unknown.progress.reads == 1 &&
        unknown.progress.callbacks_completed == 1 &&
        unknown.progress.restored_return_address.known_mask == 0x03 &&
        unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp && unknown.progress.stopped_pc == 0x8002dc50u);
}

void refusal_malformed_and_budget_prefixes() {
    Fixture missing;
    missing.context.io = nullptr;
    check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.progress.operations == 2 && missing.progress.stores == 1 &&
        !missing.progress.callbacks_completed &&
        missing.progress.stopped_pc == 0x8002dc40u &&
        missing.progress.stopped_entry == 0x80068bf8u);

    Fixture refused;
    refused.refuse = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls.size() == 1 &&
        refused.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xcafebabeu && !refused.progress.callbacks_completed);

    Fixture malformed;
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.calls.size() == 1 && !malformed.progress.callbacks_completed);

    static constexpr std::uint32_t stopped_pc[3] = {
        0x8002dc3cu, 0x8002dc40u, 0x8002dc48u};
    for (std::size_t budget = 0; budget < 3; ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == stopped_pc[budget]);
        check(f.progress.stores == (budget ? 1u : 0u) &&
            f.progress.callbacks_completed == (budget == 2 ? 1u : 0u) &&
            f.calls.size() == (budget == 2 ? 1u : 0u));
    }
}

void guest_address_and_mapping_failures() {
    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0, 0};
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        !unknown_sp.progress.operations &&
        unknown_sp.progress.stopped_pc == 0x8002dc38u);

    Fixture unaligned;
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 1 && unaligned.progress.accesses == 1 &&
        unaligned.progress.stopped_pc == 0x8002dc3cu);

    Fixture missing;
    missing.context.memory.count = 0;
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.operations == 1 &&
        missing.progress.stopped_address == EntrySp - 8u);

    Fixture bad_known;
    bad_known.known[EntrySp - 8u - Stack] = 2;
    check(bad_known.run() == NBA97_TEXT_ARGUMENT &&
        bad_known.progress.operations == 1);

    Fixture overlap;
    Nba97GameTextRegion duplicate[2] = {overlap.region, overlap.region};
    overlap.context.memory = {duplicate, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        !overlap.progress.operations);

    Fixture invalid_register;
    invalid_register.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
        {1, 0x0f};
    check(invalid_register.run() == NBA97_TEXT_ARGUMENT &&
        !invalid_register.progress.operations);

    Nba97GameLoopEntryProgress progress{};
    check(nba97_game_loop_entry(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture valid;
    check(nba97_game_loop_entry(&valid.context, nullptr) == NBA97_TEXT_ARGUMENT);
}

void wrapping_stack_pointer() {
    std::array<std::uint8_t, 0x20> bytes{}, known{};
    known.fill(1);
    Nba97GameTextRegion region{0, bytes.data(), known.data(), bytes.size()};
    Nba97GameLoopEntryContext context{};
    context.memory = {&region, 1};
    context.operation_budget = 3;
    context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 0x0f};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x12345678u, 0x0f};
    context.io = [](void*, const Nba97GameTextMemory*,
        const Nba97GameLoopEntryEvent*,
        Nba97GameMatchInitializeRegisters*) -> int { return 1; };
    Nba97GameLoopEntryProgress progress{};
    check(nba97_game_loop_entry(&context, &progress) == NBA97_TEXT_COMPLETE &&
        progress.completed && progress.frame_stack_pointer == 0xfffffff0u &&
        progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 8 &&
        progress.restored_return_address.word == 0x12345678u);
}
}

int main() {
    exact_call_and_return();
    live_stack_and_unknown_return();
    refusal_malformed_and_budget_prefixes();
    guest_address_and_mapping_failures();
    wrapping_stack_pointer();
    std::printf("game_loop_entry: %u checks passed\n", checks);
}
