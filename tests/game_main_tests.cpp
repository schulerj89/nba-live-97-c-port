#include "recovered/game_main.h"
#include "recovered/game_overlay_entry.h"
#include "recovered/game_static_initializers.h"
#include "recovered/game_global_pointer_save.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game main check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807ffff8u;
constexpr std::uint32_t FrameSp = EntrySp - 0x28u;

struct Fixture {
    enum Mode {
        Transfer,
        Return,
        Refuse,
        MissingImage,
        MissingSize,
        DirectTransfer,
        InvalidOutcome,
        UnknownEntry,
        UnalignedEntry
    } mode = Transfer;
    unsigned fail_call = 0;
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x200000, 0xcd);
    std::vector<std::uint8_t> ram_known = std::vector<std::uint8_t>(0x200000, 1);
    std::vector<std::uint8_t> stack = std::vector<std::uint8_t>(0x100, 0xcd);
    std::vector<std::uint8_t> stack_known = std::vector<std::uint8_t>(0x100, 1);
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameMainContext context{{regions, 2}, 1000, EntrySp, 0x11223344u,
        {0xa0a0a0a0u, 0xb1b1b1b1u, 0xc2c2c2c2u}, 0x800d79c8u, io, this};
    Nba97GameMainProgress progress{};
    Nba97GameStaticInitializersProgress static_progress{};
    Nba97GameGlobalPointerSaveProgress global_pointer_progress{};
    std::vector<Nba97GameMainEvent> calls;
    bool compose_static = false;
    bool compose_global_pointer = false;

    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base && std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        check(false);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base && std::uint64_t(address - region.base) < region.size)
                return region.known + (address - region.base);
        check(false);
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) {
            *byte(address + i) = std::uint8_t(value >> (i * 8));
            *known(address + i) = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(*byte(address + i)) << (i * 8);
        return value;
    }
    static int io(void* user, const Nba97GameTextMemory* memory, const Nba97GameMainEvent* event,
        Nba97GameMainValue* value, Nba97GameMainCalleeOutcome* outcome) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if (f.compose_static && event->entry == 0x800948d0u) {
            Nba97GameStaticInitializersContext context{*memory,100,event->stack_pointer,
                event->return_address,{event->saved_register[0],event->saved_register[1]}};
            if (nba97_game_static_initializers(&context,&f.static_progress) != NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.compose_global_pointer && event->entry == 0x800a4830u) {
            Nba97GameGlobalPointerSaveContext context{*memory,10,event->global_pointer};
            if (nba97_game_global_pointer_save(&context,&f.global_pointer_progress) !=
                NBA97_TEXT_COMPLETE)
                return 0;
        }
        if (f.mode == Refuse && f.calls.size() == f.fail_call)
            return 0;
        if (f.mode == InvalidOutcome && f.calls.size() == f.fail_call) {
            *outcome = static_cast<Nba97GameMainCalleeOutcome>(9);
            return 1;
        }
        if (f.mode == DirectTransfer && f.calls.size() == f.fail_call) {
            *outcome = NBA97_GAME_MAIN_CALLEE_TRANSFERRED;
            return 1;
        }
        *outcome = NBA97_GAME_MAIN_CALLEE_RETURNED;
        if (event->entry == 0x80029bfcu) {
            value->word = 0x80123400u;
            value->known = f.mode == MissingImage ? 0 : 1;
        } else if (event->entry == 0x80090d60u) {
            value->word = 0x1410u;
            value->known = f.mode == MissingSize ? 0 : 1;
        } else if (event->entry == 0x800aa468u) {
            if (f.mode != UnknownEntry) {
                const auto entry = f.mode == UnalignedEntry ? 0x801e0102u : 0x801e0100u;
                f.put(0x801e0000u, entry);
            } else {
                for (unsigned i = 0; i < 4; ++i)
                    *f.known(0x801e0000u + i) = 0;
            }
        } else if (event->kind == NBA97_GAME_MAIN_INDIRECT_CALL) {
            if (f.mode == Transfer)
                *outcome = NBA97_GAME_MAIN_CALLEE_TRANSFERRED;
            else if (f.mode == Return) {
                f.put(FrameSp + 0x24u, 0x55667788u);
                f.put(FrameSp + 0x20u, 0x12121212u);
                f.put(FrameSp + 0x1cu, 0x34343434u);
                f.put(FrameSp + 0x18u, 0x56565656u);
            }
        }
        return 1;
    }
    int run() { return nba97_game_main(&context, &progress); }
};

void transferred_path() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.completed && f.progress.transferred && f.progress.loaded_feload &&
        f.progress.reached_match_orchestration);
    check(f.calls.size() == 77 && f.progress.callbacks_completed == 77);
    check(f.progress.stores == 15 && f.progress.reads == 1 && f.progress.accesses == 16 &&
        f.progress.operations == 93);
    check(f.progress.frame_stack_pointer == FrameSp && f.progress.stack_pointer == FrameSp &&
        f.progress.global_pointer == 0x800d79c8u);
    check(f.get(FrameSp + 0x24u) == 0x11223344u &&
        f.get(FrameSp + 0x20u) == 0xc2c2c2c2u &&
        f.get(FrameSp + 0x1cu) == 0xb1b1b1b1u &&
        f.get(FrameSp + 0x18u) == 0xa0a0a0a0u);
    check(f.get(0x800d7b04u) == 0 && f.get(0x8002148cu, 2) == 0 &&
        f.get(0x800d7a94u) == 0x78u && f.get(0x800d7af4u) == 0 &&
        f.get(0x800d7af8u) == 0);
    check(f.get(FrameSp + 0x10u, 2) == 0x200u && f.get(FrameSp + 0x12u, 2) == 0 &&
        f.get(FrameSp + 0x14u, 2) == 0x200u && f.get(FrameSp + 0x16u, 2) == 0x100u);
    check(f.progress.loaded_image == 0x80123400u && f.progress.loaded_image_size == 0x1410u &&
        f.progress.indirect_entry == 0x801e0100u);
    check(f.calls.front().pc == 0x800299a4u && f.calls.front().entry == 0x800948d0u &&
        f.calls.front().return_address == 0x800299acu && f.calls.front().stack_pointer == FrameSp);
    check(f.calls[2].entry == 0x8008fa6cu && f.calls[2].argument_count == 3 &&
        f.calls[2].argument[0] == 0xdcu && f.calls[2].argument[1] == 0x8010b61cu &&
        f.calls[2].argument[2] == 0xf21e4u);
    check(f.calls[4].entry == 0x800a35d8u && f.calls[4].argument[0] == 0x800247e4u &&
        f.calls[5].entry == 0x80092c7cu && f.calls[5].argument[0] == 0x8001000cu &&
        f.calls[5].argument[1] == 0x2c3u);
    check(f.calls[18].pc == 0x80029a94u && f.calls[18].argument[0] == FrameSp + 0x10u &&
        f.calls[19].argument[2] == 0x100u);
    check(f.calls[24].entry == 0x8002d8d4u && f.calls[26].entry == 0x80029bfcu &&
        f.calls[26].argument[0] == 0x800247ecu);
    for (unsigned i = 0; i < 20; ++i)
        check(f.calls[28 + i].pc == 0x80029b20u && f.calls[28 + i].saved_register[0] == i + 1);
    check(f.calls[48].entry == 0x8009dba0u && f.calls[49].entry == 0x8009dbe0u &&
        f.calls[50].entry == 0x8009dbf8u);
    for (unsigned i = 0; i < 20; ++i)
        check(f.calls[51 + i].pc == 0x80029b50u && f.calls[51 + i].saved_register[0] == i + 1);
    check(f.calls[75].entry == 0x800aa468u && f.calls[75].argument[0] == 0x80123400u &&
        f.calls[75].argument[1] == 0x801e0000u && f.calls[75].argument[2] == 0x1410u);
    check(f.calls[76].kind == NBA97_GAME_MAIN_INDIRECT_CALL &&
        f.calls[76].entry == 0x801e0100u && f.calls[76].return_address == 0x80029bb0u &&
        f.calls[76].saved_register[0] == 20u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address && !f.progress.stopped_entry);
}

void returning_epilogue() {
    Fixture f;
    f.mode = Fixture::Return;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed && !f.progress.transferred);
    check(f.progress.operations == 97 && f.progress.accesses == 20 && f.progress.reads == 5);
    check(f.progress.stack_pointer == EntrySp && f.progress.restored_return_address == 0x55667788u);
    check(f.progress.saved_register[0] == 0x56565656u &&
        f.progress.saved_register[1] == 0x34343434u &&
        f.progress.saved_register[2] == 0x12121212u);
}

void refusals_and_unknowns() {
    { Fixture f; f.mode = Fixture::Refuse; f.fail_call = 25;
      check(f.run() == NBA97_TEXT_IO_REFUSED && f.calls.size() == 25 &&
          f.progress.stopped_pc == 0x80029adcu && f.progress.stopped_entry == 0x8002d8d4u &&
          f.progress.callbacks_completed == 24 && f.progress.reached_match_orchestration); }
    { Fixture f; f.mode = Fixture::DirectTransfer; f.fail_call = 1;
      check(f.run() == NBA97_TEXT_ARGUMENT && f.progress.stopped_pc == 0x800299a4u &&
          !f.progress.callbacks_completed); }
    { Fixture f; f.mode = Fixture::InvalidOutcome; f.fail_call = 2;
      check(f.run() == NBA97_TEXT_ARGUMENT && f.progress.stopped_pc == 0x800299acu &&
          f.progress.callbacks_completed == 1); }
    { Fixture f; f.mode = Fixture::MissingImage;
      check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80029b04u &&
          f.progress.callbacks_completed == 27); }
    { Fixture f; f.mode = Fixture::MissingSize;
      check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80029b10u &&
          f.progress.callbacks_completed == 28); }
    { Fixture f; f.mode = Fixture::UnknownEntry;
      check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80029ba0u &&
          f.progress.stopped_address == 0x801e0000u && f.progress.callbacks_completed == 76); }
    { Fixture f; f.mode = Fixture::UnalignedEntry;
      check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP && f.progress.stopped_pc == 0x80029ba8u &&
          f.progress.stopped_entry == 0x801e0102u && f.progress.callbacks_completed == 76); }
}

void memory_and_budget() {
    { Fixture f; f.context.operation_budget = 0;
      check(f.run() == NBA97_TEXT_LIMIT && f.progress.stopped_pc == 0x80029998u &&
          f.progress.stopped_address == FrameSp + 0x24u && !f.progress.operations); }
    { Fixture f; f.context.operation_budget = 4;
      check(f.run() == NBA97_TEXT_LIMIT && f.progress.stores == 4 &&
          f.progress.stopped_pc == 0x800299a4u && f.progress.callbacks_completed == 0); }
    { Fixture f; f.context.operation_budget = 92;
      check(f.run() == NBA97_TEXT_LIMIT && f.progress.callbacks_completed == 76 &&
          f.progress.stopped_pc == 0x80029ba8u && f.progress.stopped_entry == 0x801e0100u); }
    { Fixture f; f.regions[1].size = 0x20;
      check(f.run() == NBA97_TEXT_RESOURCE && f.progress.stopped_pc == 0x80029998u); }
    { Fixture f; *f.known(FrameSp + 0x24u) = 2;
      check(f.run() == NBA97_TEXT_ARGUMENT && !f.progress.stores); }
    { Fixture f; Nba97GameTextRegion overlap[2] = {f.regions[0], f.regions[0]};
      f.context.memory = {overlap, 2}; check(f.run() == NBA97_TEXT_ARGUMENT && !f.progress.operations); }
    Nba97GameMainProgress progress{};
    check(nba97_game_main(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_main(&f.context, nullptr) == NBA97_TEXT_ARGUMENT);
}

struct Composition {
    Fixture game;
    Nba97GameOverlayEntryProgress overlay_progress{};
    Nba97GameMainProgress main_progress{};
    Nba97GameOverlayEntryContext overlay{{game.regions, 2}, 100000,
        0x99887766u, overlayIo, this};
    Composition() {
        game.put(0x800c4b3cu, 0x00800000u);
        game.put(0x800c4b38u, 0x00008000u);
        game.put(0x800c4b14u, 0);
        game.compose_static = true;
        game.compose_global_pointer = true;
    }
    static int overlayIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameOverlayEntryEvent* event, Nba97GameOverlayEntryCalleeOutcome* outcome) {
        auto& self = *static_cast<Composition*>(user);
        if (event->kind == NBA97_GAME_OVERLAY_BIOS_A0_39_INIT_HEAP) {
            *outcome = NBA97_GAME_OVERLAY_CALLEE_RETURNED;
            return 1;
        }
        self.game.context.memory = *memory;
        self.game.context.stack_pointer = event->stack_pointer;
        self.game.context.return_address = event->return_address;
        self.game.context.global_pointer = event->global_pointer;
        self.game.context.saved_register[0] = 0;
        self.game.context.saved_register[1] = 0;
        self.game.context.saved_register[2] = 0;
        const auto result = nba97_game_main(&self.game.context, &self.main_progress);
        if (result != NBA97_TEXT_COMPLETE || !self.main_progress.transferred)
            return 0;
        *outcome = NBA97_GAME_OVERLAY_CALLEE_TRANSFERRED;
        return 1;
    }
};

void overlay_composition() {
    Composition c;
    check(nba97_game_overlay_entry(&c.overlay, &c.overlay_progress) == NBA97_TEXT_COMPLETE);
    check(c.overlay_progress.completed && c.overlay_progress.transferred &&
        c.overlay_progress.entered_main && c.main_progress.completed && c.main_progress.transferred);
    check(c.main_progress.frame_stack_pointer == 0x807fffd0u && c.game.calls.size() == 77);
    check(c.game.static_progress.completed && c.game.static_progress.initialized &&
        c.game.get(0x800c4b14u) == 1);
    check(c.game.global_pointer_progress.completed &&
        c.game.global_pointer_progress.stored_global_pointer == 0x800d79c8u &&
        c.game.get(0x800d6e2cu) == 0x800d79c8u);
    check(c.game.get(0x800d7bb8u) == 0x99887766u &&
        c.overlay_progress.restored_return_address == 0x99887766u);
}
}

int main() {
    transferred_path();
    returning_epilogue();
    refusals_and_unknowns();
    memory_and_budget();
    overlay_composition();
    std::printf("game_main: %u checks passed\n", checks);
}
