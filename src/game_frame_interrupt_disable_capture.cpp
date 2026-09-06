#include "game_frame_interrupt_disable_adapter.h"
#include "game_frame_interrupt_disable_capture.h"
#include <sstream>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {
using U32 = std::uint32_t;
constexpr std::array<U32, 4> DisablePcs{{
    0x80049070u, 0x800491c8u, 0x8004920cu, 0x8004927cu}};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x200040);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x200040);
    std::vector<Nba97MatchFrameCall> fallback_calls;
    std::vector<U32> restore_arguments;
    std::vector<Nba97GameFrameInterruptDisableWord> before_restore;
    Nba97GameFrameInterruptDisableBinding binding{};
    Nba97MatchFrameProgress frame_progress{};
    U32 fail_pc = 0;

    static std::size_t offset(U32 address, unsigned width) {
        if (address >= 0x80000000u &&
            std::uint64_t(address) + width <= 0x80200000u)
            return address - 0x80000000u;
        if (address >= 0x1f800000u &&
            std::uint64_t(address) + width <= 0x1f800040u)
            return 0x200000u + address - 0x1f800000u;
        throw std::out_of_range("unowned memory");
    }

    Fixture(U32 status = 0xabcdef01u, std::uint8_t mask = 15) {
        binding.cp0_status = {status, mask};
        binding.operation_budget = 2;
        put(0x8001ede8u, 0);
        put(0x800b729cu, 384);
        put(0x800fc660u, 0x80140000u);
        put(0x80140000u, 1, 2); /* Pause skips attachment selector. */
        put(0x800b2048u, 0x80141000u);
        put(0x80141053u, 0xfe, 1);
        put(0x1f800030u, 0); /* First scratch sentinel ends redraw. */
    }

    void put(U32 address, U32 value, unsigned width = 4) {
        const auto at = offset(address, width);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
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

    static int io(void* opaque, const Nba97MatchFrameCall* call,
        Nba97GamePeriodValue*) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.fallback_calls.push_back(*call);
        if (call->pc == f.fail_pc)
            return NBA97_BODY_BOUNDS;
        if (call->entry == 0x8004900cu) {
            f.before_restore.push_back(f.binding.cp0_status);
            f.restore_arguments.push_back(call->args[0]);
            /* Explicit typed fixture for queued restore boundary 0x8004900C. */
            f.binding.cp0_status = {call->args[0], 15};
        }
        return NBA97_BODY_OK;
    }

    int run(bool with_io = true) {
        Nba97MatchFrameContext frame{access, with_io ? io : nullptr, this,
            10000};
        return nba97_game_match_frame_with_interrupt_disable(
            &frame, &binding, &frame_progress);
    }
};

} // namespace
namespace nba97 {
std::string captureGameFrameInterruptDisable() {
  Fixture f;
  if(f.run()!=NBA97_BODY_OK || !f.frame_progress.completed || f.binding.completions!=13 || f.before_restore.size()!=13)
    throw std::runtime_error("interrupt-disable native frame fixture failed");
  for(const auto& status:f.before_restore)
    if(status.word!=0xabcdef00 || status.known_mask!=15)
      throw std::runtime_error("interrupt-disable CP0 write drifted");
  for(const auto& status:f.restore_arguments)
    if(status!=0xabcdef01)throw std::runtime_error("interrupt-disable old Status drifted");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80048FF4\",\"inclusive_end\":\"0x8004900B\","
       "\"bytes\":24,\"instructions\":6,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual match frame; explicit CP0 state; typed restore and rendering fixtures\","
       "\"completed\":true,\"status_before\":2882400001,\"status_disabled\":2882400000,"
       "\"invocations\":" << f.binding.invocations << ",\"completions\":" << f.binding.completions
    << ",\"status_after_typed_restore\":" << f.binding.cp0_status.word << ",\"call_counts\":[";
  for(unsigned i=0;i<4;++i){if(i)o<<',';o<<f.binding.call_count[i];}
  o << "],\"call_pcs\":[2147782768,2147783112,2147783180,2147783292],\"operations_per_call\":2,\"frame_completed\":true}";
  return o.str();
}
}
