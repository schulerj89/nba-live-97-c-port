#include "game_frame_interrupt_restore_adapter.h"
#include "game_frame_interrupt_restore_capture.h"
#include <sstream>
#include "game_frame_interrupt_disable_adapter.h"

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
constexpr std::array<U32, 4> RestorePcs{{
    0x8004909cu, 0x800491d8u, 0x8004926cu, 0x800492c0u}};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x200040);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x200040);
    std::vector<Nba97MatchFrameCall> typed_calls;
    std::vector<Nba97GameFrameInterruptRestoreWord> after_disable;
    Nba97GameFrameInterruptDisableBinding disable{};
    Nba97GameFrameInterruptRestoreBinding restore{};
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

    Fixture(U32 status = 0xabcdef01u) {
        disable.cp0_status = {status, 15};
        disable.operation_budget = 2;
        restore.cp0_status = {status, 15};
        restore.operation_budget = 1;
        put(0x8001ede8u, 0);
        put(0x800b729cu, 384);
        put(0x800fc660u, 0x80140000u);
        put(0x80140000u, 1, 2);
        put(0x800b2048u, 0x80141000u);
        put(0x80141053u, 0xfe, 1);
        put(0x1f800030u, 0);
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
        Nba97GamePeriodValue* value) {
        auto& f = *static_cast<Fixture*>(opaque);
        if (call->entry == 0x80048ff4u) {
            f.disable.cp0_status = {f.restore.cp0_status.word,
                f.restore.cp0_status.known_mask};
            const int result =
                nba97_game_frame_interrupt_disable_from_match_frame(
                    &f.disable, call, value);
            f.restore.cp0_status = {f.disable.cp0_status.word,
                f.disable.cp0_status.known_mask};
            f.after_disable.push_back(f.restore.cp0_status);
            return result;
        }
        f.typed_calls.push_back(*call);
        if (call->pc == f.fail_pc)
            return NBA97_BODY_BOUNDS;
        return NBA97_BODY_OK;
    }

    int run(bool with_io = true) {
        Nba97MatchFrameContext frame{access, with_io ? io : nullptr, this,
            10000};
        return nba97_game_match_frame_with_interrupt_restore(
            &frame, &restore, &frame_progress);
    }
};

} // namespace
namespace nba97 {
std::string captureGameFrameInterruptRestore() {
  Fixture f;
  if(f.run()!=NBA97_BODY_OK || !f.frame_progress.completed || f.disable.completions!=13 || f.restore.completions!=13 || f.after_disable.size()!=13)
    throw std::runtime_error("interrupt-restore native frame fixture failed");
  for(const auto& status:f.after_disable)
    if(status.word!=0xabcdef00 || status.known_mask!=15)
      throw std::runtime_error("interrupt-restore preceding CP0 state drifted");
  for(unsigned i=0;i<4;++i)
    if(f.restore.progress[i].published_status.word!=0xabcdef01 || f.restore.progress[i].published_status.known_mask!=15)
      throw std::runtime_error("interrupt-restore published CP0 state drifted");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8004900C\",\"inclusive_end\":\"0x80049017\","
       "\"bytes\":12,\"instructions\":3,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual match frame with recovered disable and restore; explicit CP0 state; typed rendering fixtures\","
       "\"completed\":true,\"status_disabled\":2882400000,"
       "\"disable_completions\":" << f.disable.completions << ",\"restore_completions\":" << f.restore.completions
    << ",\"status_restored\":" << f.restore.cp0_status.word << ",\"call_counts\":[";
  for(unsigned i=0;i<4;++i){if(i)o<<',';o<<f.restore.call_count[i];}
  o << "],\"call_pcs\":[";
  for(unsigned i=0;i<4;++i){if(i)o<<',';o<<f.restore.event[i].pc;}
  o << "],\"operations_per_call\":" << f.restore.progress[0].operations << ",\"frame_completed\":true}";
  return o.str();
}
}
