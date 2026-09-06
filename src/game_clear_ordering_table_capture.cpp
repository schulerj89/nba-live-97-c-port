#include "game_clear_ordering_table_adapter.h"
#include "game_clear_ordering_table_capture.h"
#include <sstream>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {
using U32 = std::uint32_t;

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x200040);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x200040);
    std::array<Nba97GameTextRegion, 2> owner_regions{};
    std::array<Nba97GameClearOrderingTableAccess, 32> journal{};
    std::vector<Nba97MatchFrameCall> fallback_calls;
    std::vector<Nba97GameClearOrderingTableEvent> owner_calls;
    std::vector<Nba97GameClearOrderingTableMachine> owner_entries;
    Nba97GameClearOrderingTableMatchFrameBinding binding{};
    Nba97MatchFrameProgress frame_progress{};
    unsigned provider_calls = 0;
    bool refuse_provider = false;
    bool refuse_backend = false;
    U32 status = 0xabcdef01u;

    static std::size_t offset(U32 address, unsigned width) {
        if (address >= 0x80000000u &&
            std::uint64_t(address) + width <= 0x80200000u)
            return address - 0x80000000u;
        if (address >= 0x1f800000u &&
            std::uint64_t(address) + width <= 0x1f800040u)
            return 0x200000u + address - 0x1f800000u;
        throw std::out_of_range("unowned memory");
    }

    void put(U32 address, U32 value, unsigned width = 4) {
        const auto at = offset(address, width);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
    }

    U32 get(U32 address, unsigned width = 4) const {
        const auto at = offset(address, width);
        U32 value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= U32(bytes[at + i]) << (i * 8u);
        return value;
    }

    Fixture() {
        owner_regions[0] = {0x80000000u, bytes.data(), known.data(),
            0x200000u};
        owner_regions[1] = {0x1f800000u, bytes.data() + 0x200000u,
            known.data() + 0x200000u, 0x40u};
        binding.memory = {owner_regions.data(), owner_regions.size()};
        binding.operation_budget = 32;
        binding.entry_machine_provider = provideMachine;
        binding.entry_machine_user = this;
        binding.io = ownerIo;
        binding.user = this;
        binding.access_journal = journal.data();
        binding.access_journal_capacity = journal.size();

        put(0x8001ede8u, 0);
        put(0x800b729cu, 384);
        put(0x800fc660u, 0x80140000u);
        put(0x80140000u, 1, 2); /* Pause skips the selector pointer. */
        put(0x800b2048u, 0x80141000u);
        put(0x80141053u, 0xfe, 1);
        put(0x1f800030u, 0); /* First sentinel ends redraw. */
        put(0x800c55c2u, 0, 1);
        put(0x800c55b8u, 0x800c5578u);
        put(0x800c5578u + 0x2cu, 0x8009a97cu);
    }

    static int access(void* opaque, U32, U32 address, unsigned width,
        unsigned kind, Nba97PlayerFrameValue* value) {
        auto& f = *static_cast<Fixture*>(opaque);
        try {
            const auto at = offset(address, width);
            if (kind) {
                f.put(address, value->word, width);
            } else {
                *value = {};
                for (unsigned i = 0; i < width; ++i)
                    if (f.known[at + i]) {
                        value->word |= U32(f.bytes[at + i]) << (i * 8u);
                        value->known_mask = static_cast<std::uint8_t>(
                            value->known_mask | (1u << i));
                    }
            }
            return NBA97_BODY_OK;
        } catch (const std::out_of_range&) {
            return NBA97_BODY_BOUNDS;
        }
    }

    static int fallback(void* opaque, const Nba97MatchFrameCall* call,
        Nba97GamePeriodValue* value) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.fallback_calls.push_back(*call);
        if (call->entry == 0x80048ff4u) {
            *value = {f.status, 1};
            f.status &= ~1u;
        } else if (call->entry == 0x8004900cu) {
            f.status = call->args[0];
        }
        return NBA97_BODY_OK;
    }

    static int provideMachine(void* opaque, const Nba97MatchFrameCall* call,
        std::size_t invocation, Nba97GameClearOrderingTableMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        ++f.provider_calls;
        if (f.refuse_provider)
            return 0;
        *machine = {};
        for (unsigned r = 0; r < 32; ++r)
            machine->registers.gpr[r] =
                {U32(0x10000000u * invocation + r), 0x0f};
        machine->registers.gpr[0] = {0, 0x0f};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {U32(0x80180000u + invocation * 0x100u), 0x0f};
        /* Deliberately wrong values prove the adapter derives these four
         * registers from the exact parent event rather than trusting them. */
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {1, 0};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {2, 0};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {3, 0};
        machine->hi = {U32(0xabc00000u + invocation), 0x0f};
        machine->lo = {U32(0xdef00000u + invocation), 0x0f};
        if(call->pc != (invocation == 1 ? 0x80049084u : 0x80049094u))
            throw std::runtime_error("clear-table call PC drifted");
        return 1;
    }

    static int ownerIo(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameClearOrderingTableEvent* event,
        Nba97GameClearOrderingTableMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.owner_calls.push_back(*event);
        f.owner_entries.push_back(*machine);
        if (f.refuse_backend)
            return 0;
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            {0xdeadbeefu, 0x0f};
        return 1;
    }

    int run() {
        Nba97MatchFrameContext frame{access, fallback, this, 10000};
        return nba97_game_match_frame_with_clear_ordering_table(
            &frame, &binding, &frame_progress);
    }
};

} // namespace
namespace nba97 {
std::string captureGameClearOrderingTable() {
  Fixture f;
  if(f.run()!=NBA97_BODY_OK || !f.frame_progress.completed || f.binding.completions!=2 ||
      f.get(0x800fccf0)!=0x000c567c || f.get(0x800f5c50)!=0x000c567c)
    throw std::runtime_error("clear-table native frame fixture failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80099960\",\"inclusive_end\":\"0x800999F7\","
       "\"bytes\":152,\"instructions\":38,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual match frame; independent full entry machines; typed clear backend and rendering fixtures\","
       "\"completed\":true,\"frame_completed\":true,\"heads_before\":[0,0],\"heads_after\":["
    << f.get(0x800fccf0) << ',' << f.get(0x800f5c50) << "],\"calls\":[";
  for(unsigned i=0;i<2;++i){
    if(i)o<<',';
    const auto& q=f.binding.progress[i];
    o << "{\"pc\":" << f.binding.event[i].pc << ",\"count\":" << f.binding.event[i].args[1]
      << ",\"target\":" << q.backend_target.word << ",\"operations\":" << q.operations
      << ",\"returned_sp\":" << q.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word
      << ",\"restored_ra\":" << q.restored_return_address.word << "}";
  }
  o << "]}"; return o.str();
}
}
