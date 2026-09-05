#include "recovered/feload_entry.h"
#include "recovered/game_main.h"
#include "recovered/game_memory_copy.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

unsigned checks;

void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "feload entry integration check %u failed\n",
            checks);
        std::abort();
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t RamSize = 0x00200000u;
constexpr std::uint32_t EntrySp = 0x801ff000u;
constexpr std::uint32_t Image = 0x80123400u;
constexpr std::uint32_t OverlayBase = 0x801e0000u;
constexpr std::uint32_t FeloadEntry = 0x801e1410u;
constexpr std::uint32_t Bss = 0x801e903cu;
constexpr std::uint32_t BssEnd = 0x801eb088u;

struct Composition {
    enum ChildMode { TransferSecond, ReturnSecond } child_mode;
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(RamSize, 0xcd);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(RamSize, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameMainContext main_context{};
    Nba97GameMainProgress main_progress{};
    Nba97GameMemoryCopyProgress copy_progress{};
    Nba97FeloadEntryProgress feload_progress{};
    int feload_result = 0;
    std::vector<Nba97GameMainEvent> main_calls;
    std::vector<Nba97FeloadEntryEvent> feload_calls;

    explicit Composition(ChildMode mode) : child_mode(mode) {
        main_context.memory = {&region, 1};
        main_context.operation_budget = 1000;
        main_context.stack_pointer = EntrySp;
        main_context.return_address = 0x11223344u;
        main_context.saved_register[0] = 0xa0a0a0a0u;
        main_context.saved_register[1] = 0xb1b1b1b1u;
        main_context.saved_register[2] = 0xc2c2c2c2u;
        main_context.global_pointer = 0x800d79c8u;
        main_context.io = main_io;
        main_context.user = this;
        put(Image, FeloadEntry);
        put(OverlayBase, 0xccccccccu);
        put(0x801e8b70u, 0x00200000u);
        put(0x801e8b6cu, 0x00004000u);
        for (std::uint32_t address = Bss; address < BssEnd; address += 4u)
            put(address, 0xa5a5a5a5u);
    }

    void put(std::uint32_t address, std::uint32_t value) {
        check(address >= Ram && std::uint64_t(address - Ram) + 4u <= RamSize);
        for (unsigned i = 0; i < 4; ++i) {
            bytes[address - Ram + i] = std::uint8_t(value >> (i * 8u));
            known[address - Ram + i] = 1;
        }
    }

    std::uint32_t get(std::uint32_t address) const {
        check(address >= Ram && std::uint64_t(address - Ram) + 4u <= RamSize);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[address - Ram + i]) << (i * 8u);
        return value;
    }

    static int child_io(void* user, const Nba97GameTextMemory*,
        const Nba97FeloadEntryEvent* event,
        Nba97FeloadEntryRegisters*, Nba97FeloadEntryCalleeOutcome* outcome) {
        auto& self = *static_cast<Composition*>(user);
        self.feload_calls.push_back(*event);
        if (event->kind == NBA97_FELOAD_ENTRY_CHILD_801E1590) {
            *outcome = NBA97_FELOAD_ENTRY_CALLEE_RETURNED;
            return 1;
        }
        *outcome = self.child_mode == TransferSecond
            ? NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED
            : NBA97_FELOAD_ENTRY_CALLEE_RETURNED;
        return 1;
    }

    static int main_io(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMainEvent* event, Nba97GameMainValue* value,
        Nba97GameMainCalleeOutcome* outcome) {
        auto& self = *static_cast<Composition*>(user);
        self.main_calls.push_back(*event);
        *outcome = NBA97_GAME_MAIN_CALLEE_RETURNED;
        if (event->entry == 0x80029bfcu) {
            value->word = Image;
            value->known = 1;
        } else if (event->entry == 0x80090d60u) {
            value->word = 4;
            value->known = 1;
        } else if (event->entry == 0x800aa468u) {
            Nba97GameMemoryCopyContext copy{*memory, 100,
                event->argument[0], event->argument[1], event->argument[2]};
            if (nba97_game_memory_copy(&copy, &self.copy_progress) !=
                    NBA97_TEXT_COMPLETE || !self.copy_progress.completed)
                return 0;
            value->word = self.copy_progress.return_v0;
            value->known = self.copy_progress.return_v0_known;
        } else if (event->kind == NBA97_GAME_MAIN_INDIRECT_CALL) {
            if (event->entry != FeloadEntry)
                return 0;
            Nba97FeloadEntryContext feload{};
            feload.memory = *memory;
            feload.operation_budget = 10000;
            feload.registers.gpr[NBA97_FELOAD_R_SP] =
                {event->stack_pointer, 1};
            feload.registers.gpr[NBA97_FELOAD_R_GP] =
                {event->global_pointer, 1};
            feload.registers.gpr[NBA97_FELOAD_R_RA] =
                {event->return_address, 1};
            feload.registers.gpr[NBA97_FELOAD_R_S0] =
                {event->saved_register[0], 1};
            feload.registers.gpr[NBA97_FELOAD_R_S1] =
                {event->saved_register[1], 1};
            feload.registers.gpr[NBA97_FELOAD_R_S2] =
                {event->saved_register[2], 1};
            feload.io = child_io;
            feload.user = &self;
            self.feload_result = nba97_feload_entry(&feload,
                &self.feload_progress);
            if (self.feload_result != NBA97_TEXT_COMPLETE ||
                !self.feload_progress.transferred)
                return 0;
            *outcome = NBA97_GAME_MAIN_CALLEE_TRANSFERRED;
        }
        return 1;
    }

    int run() {
        return nba97_game_main(&main_context, &main_progress);
    }
};

void natural_main_to_transferred_second_child() {
    Composition composition(Composition::TransferSecond);
    check(composition.run() == NBA97_TEXT_COMPLETE &&
        composition.main_progress.completed &&
        composition.main_progress.transferred &&
        composition.feload_result == NBA97_TEXT_COMPLETE &&
        composition.feload_progress.completed &&
        composition.feload_progress.transferred);
    check(composition.copy_progress.completed &&
        composition.copy_progress.requested_length == 4 &&
        composition.copy_progress.source == Image &&
        composition.copy_progress.destination == OverlayBase &&
        composition.get(OverlayBase) == FeloadEntry &&
        composition.main_progress.indirect_entry == FeloadEntry);
    check(composition.main_calls.size() == 77 &&
        composition.main_calls.back().kind == NBA97_GAME_MAIN_INDIRECT_CALL &&
        composition.main_calls.back().pc == 0x80029ba8u &&
        composition.main_calls.back().entry == FeloadEntry &&
        composition.main_calls.back().return_address == 0x80029bb0u);
    check(composition.feload_calls.size() == 2 &&
        composition.feload_calls[0].entry == 0x801e1590u &&
        composition.feload_calls[1].entry == 0x801e136cu &&
        composition.feload_progress.saved_return_address.known &&
        composition.feload_progress.saved_return_address.word == 0x80029bb0u &&
        composition.feload_progress.restored_return_address.word ==
            0x80029bb0u);
    check(composition.feload_calls[0].registers.gpr[NBA97_FELOAD_R_SP].word ==
            0x801ffff8u &&
        composition.feload_calls[0].registers.gpr[NBA97_FELOAD_R_GP].word ==
            Bss &&
        composition.feload_calls[0].registers.gpr[NBA97_FELOAD_R_A0].word ==
            0x801eb08cu &&
        composition.feload_calls[0].registers.gpr[NBA97_FELOAD_R_A1].word ==
            0x00010f70u &&
        !composition.feload_calls[0].registers.gpr[NBA97_FELOAD_R_A2].known &&
        !composition.feload_calls[0].registers.gpr[NBA97_FELOAD_R_A3].known);
    check(composition.feload_progress.words_cleared == 2067 &&
        composition.feload_progress.stores == 2070 &&
        composition.feload_progress.reads == 3 &&
        composition.feload_progress.operations == 2075 &&
        composition.get(0x801e8b50u) == 0x00010f70u &&
        composition.get(0x801e8b4cu) == BssEnd &&
        composition.get(Bss) == 0x80029bb0u);
    for (std::uint32_t address = Bss + 4u; address < BssEnd;
         address += 4u)
        check(composition.get(address) == 0);
}

void natural_main_observes_returning_child_break() {
    Composition composition(Composition::ReturnSecond);
    check(composition.run() == NBA97_TEXT_IO_REFUSED &&
        !composition.main_progress.completed &&
        composition.main_progress.stopped_pc == 0x80029ba8u &&
        composition.main_progress.stopped_entry == FeloadEntry &&
        composition.main_progress.callbacks_completed == 76);
    check(composition.copy_progress.completed &&
        composition.get(OverlayBase) == FeloadEntry &&
        composition.main_progress.indirect_entry == FeloadEntry);
    check(composition.feload_result == NBA97_FELOAD_ENTRY_BREAK_TRAP &&
        composition.feload_progress.trapped &&
        !composition.feload_progress.completed &&
        !composition.feload_progress.transferred &&
        composition.feload_progress.stopped_pc == 0x801e14b4u &&
        composition.feload_progress.first_child_entered &&
        composition.feload_progress.second_child_entered &&
        composition.feload_progress.callbacks_completed == 2 &&
        composition.feload_calls.size() == 2);
}

} // namespace

int main() {
    natural_main_to_transferred_second_child();
    natural_main_observes_returning_child_break();
    std::printf("feload_entry_integration: %u checks passed\n", checks);
}
