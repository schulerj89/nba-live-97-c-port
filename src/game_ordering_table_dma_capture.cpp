#include "game_ordering_table_dma_adapter.h"
#include "game_ordering_table_dma_capture.h"
#include <sstream>
#include <stdexcept>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;

struct Block {
    U32 base;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> known;
    Nba97GameTextRegion region{};
    Block(U32 address, std::size_t size) : base(address), data(size),
        known(size, 1) {
        region = {base, data.data(), known.data(), data.size()};
    }
    void put(U32 address, U32 value, unsigned width = 4) {
        const std::size_t at = address - base;
        for (unsigned i = 0; i < width; ++i) {
            data[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] = 1;
        }
    }
    U32 get(U32 address) const {
        U32 value = 0;
        const std::size_t at = address - base;
        for (unsigned i = 0; i < 4; ++i)
            value |= U32(data[at + i]) << (8u * i);
        return value;
    }
};

struct Fixture {
    static constexpr U32 AddressRegister = 0x1f8010e0u;
    static constexpr U32 CountRegister = 0x1f8010e4u;
    static constexpr U32 ControlRegister = 0x1f8010e8u;
    static constexpr U32 MasterRegister = 0x1f8010f0u;
    Block globals{0x800c5578u, 0x13cu};
    Block stack{0x80100000u, 512};
    Block ot{0x800f5c50u, 0x5000};
    Block mmio{0x1f8010e0u, 20};
    std::array<Nba97GameTextRegion, 4> regions{};
    std::array<Nba97GameOrderingTableDmaAccess, 64> dma_journal{};
    std::vector<Nba97GameClearOrderingTableEvent> fallback_events;
    std::vector<Nba97GameOrderingTableDmaEvent> dma_events;
    Nba97GameClearOrderingTableContext parent{};
    Nba97GameClearOrderingTableProgress parent_progress{};
    Nba97GameOrderingTableDmaBinding binding{};
    bool clear_immediately = true;
    bool wait_error = false;
    unsigned malformed_child_kind = 0;

    Fixture(U32 debug = 0, U32 count = 4096) {
        regions = {globals.region, stack.region, ot.region, mmio.region};
        globals.put(0x800c55c2u, debug, 1);
        globals.put(0x800c55bcu, 0x8009cb2cu);
        globals.put(0x800c55b8u, 0x800c5578u);
        globals.put(0x800c5578u + 0x2cu, 0x8009a97cu);
        globals.put(0x800c56a4u, AddressRegister);
        globals.put(0x800c56a8u, CountRegister);
        globals.put(0x800c56acu, ControlRegister);
        globals.put(0x800c56b0u, MasterRegister);
        mmio.put(MasterRegister, 0x12345678u);
        mmio.put(ControlRegister, 0x55667788u);
        for (unsigned r = 0; r < 32; ++r)
            parent.machine.registers.gpr[r] =
                {0x30000000u + r * 0x101u, 0x0f};
        parent.machine.registers.gpr[0] = {0, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {ot.base, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
            {count, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x80100100u, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8004909cu, 0x0f};
        parent.machine.hi = {0x89abcdefu, 0x0f};
        parent.machine.lo = {0x76543210u, 0x0f};
        parent.memory = {regions.data(), regions.size()};
        parent.operation_budget = 100;
        parent.io = fallback;
        parent.user = this;
        binding.operation_budget = 100;
        binding.io = dmaIo;
        binding.user = this;
        binding.access_journal = dma_journal.data();
        binding.access_journal_capacity = dma_journal.size();
    }

    static void putMemory(const Nba97GameTextMemory* memory, U32 address,
        U32 value) {
        for (std::size_t r = 0; r < memory->count; ++r) {
            auto& region = memory->region[r];
            if (address >= region.base &&
                std::uint64_t(address) + 4 <=
                    std::uint64_t(region.base) + region.size) {
                const std::size_t at = address - region.base;
                for (unsigned i = 0; i < 4; ++i) {
                    region.data[at + i] =
                        static_cast<std::uint8_t>(value >> (8u * i));
                    if (region.known)
                        region.known[at + i] = 1;
                }
                return;
            }
        }
        std::abort();
    }

    static int fallback(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameClearOrderingTableEvent* event,
        Nba97GameClearOrderingTableMachine*) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.fallback_events.push_back(*event);
        return 1;
    }

    static int dmaIo(void* opaque, const Nba97GameTextMemory* memory,
        const Nba97GameOrderingTableDmaEvent* event,
        Nba97GameOrderingTableDmaMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.dma_events.push_back(*event);
        if (event->kind == NBA97_GAME_ORDERING_TABLE_DMA_START) {
            if (f.clear_immediately)
                putMemory(memory, ControlRegister, 0x10000002u);
        } else {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                {f.wait_error ? 3u : 0u, 0x0f};
            if (!f.wait_error)
                putMemory(memory, ControlRegister, 0x10000002u);
        }
        if (f.malformed_child_kind == event->kind)
            machine->registers.gpr[0].known_mask = 0;
        return 1;
    }

    int run() {
        return nba97_game_clear_ordering_table_with_dma(
            &parent, &binding, &parent_progress);
    }
};

} // namespace
namespace nba97 {
std::string captureGameOrderingTableDma() {
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8009A97C\",\"inclusive_end\":\"0x8009AA63\","
       "\"bytes\":232,\"instructions\":58,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual ClearOTagR wrapper with mapped MMIO fixture and typed DMA start/wait services\","
       "\"runs\":[";
  for(unsigned error=0;error<2;++error){
    Fixture f(error?2:0,error?32:4096);f.clear_immediately=false;f.wait_error=error!=0;
    if(f.run()!=NBA97_TEXT_COMPLETE || !f.parent_progress.completed || !f.binding.progress.completed)
      throw std::runtime_error("ordering DMA native composition failed");
    const auto& q=f.binding.progress;
    if(error)o<<',';
    o << "{\"completed\":true,\"parent_completed\":true,\"operations\":" << q.operations
      << ",\"reads\":" << q.reads << ",\"stores\":" << q.stores << ",\"callbacks\":" << q.callbacks_completed
      << ",\"waits\":" << q.wait_iterations << ",\"dma_address\":" << f.mmio.get(Fixture::AddressRegister)
      << ",\"dma_count\":" << f.mmio.get(Fixture::CountRegister)
      << ",\"master_before\":305419896,\"master_after\":" << f.mmio.get(Fixture::MasterRegister)
      << ",\"control_before\":1432778632,\"control_started\":" << q.started_channel_control.word
      << ",\"control_after\":" << f.mmio.get(Fixture::ControlRegister)
      << ",\"head_before\":0,\"head_after\":" << f.ot.get(f.ot.base)
      << ",\"backend_return\":" << q.return_v0.word << ",\"parent_return\":" << f.parent_progress.return_v0.word
      << ",\"returned_sp\":" << q.machine.registers.gpr[29].word << ",\"restored_ra\":" << q.restored_return_address.word << "}";
  }
  o << "]}";return o.str();
}
}
