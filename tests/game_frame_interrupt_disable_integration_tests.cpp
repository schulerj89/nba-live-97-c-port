#include "game_frame_interrupt_disable_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game frame interrupt-disable integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

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

void actualFrameAllFourSites() {
    Fixture f;
    check(f.run() == NBA97_BODY_OK && f.frame_progress.completed &&
        f.binding.invocations == 13 && f.binding.completions == 13 &&
        f.before_restore.size() == 13 && f.restore_arguments.size() == 13);
    const std::array<std::size_t, 4> expected_counts{{1, 10, 1, 1}};
    for (unsigned i = 0; i < DisablePcs.size(); ++i) {
        const auto& event = f.binding.event[i];
        const auto& progress = f.binding.progress[i];
        check(event.pc == DisablePcs[i] && event.entry == 0x80048ff4u &&
            event.args[0] == 0 && event.args[1] == 0 &&
            f.binding.result[i] == NBA97_TEXT_COMPLETE && progress.completed &&
            f.binding.call_count[i] == expected_counts[i]);
        check(progress.old_status.word == 0xabcdef01u &&
            progress.old_status.known_mask == 15 &&
            progress.new_status.word == 0xabcdef00u &&
            progress.machine.cp0_status.word == 0xabcdef00u &&
            progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V0].word == 0xabcdef01u &&
            progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V1].word == 0xabcdef00u);
        check(progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_ZERO].known_mask == 15 &&
            progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].word == DisablePcs[i] + 8u &&
            progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].known_mask == 15 &&
            progress.machine.hi.known_mask == 0 &&
            progress.machine.lo.known_mask == 0);
        for (unsigned r = 1; r < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++r)
            if (r != NBA97_MATCH_INITIALIZE_V0 &&
                r != NBA97_MATCH_INITIALIZE_V1 &&
                r != NBA97_MATCH_INITIALIZE_RA)
                check(progress.machine.registers.gpr[r].known_mask == 0);
    }
    for (unsigned i = 0; i < f.before_restore.size(); ++i)
        check(f.before_restore[i].word == 0xabcdef00u &&
            f.before_restore[i].known_mask == 15 &&
            f.restore_arguments[i] == 0xabcdef01u);
    check(f.binding.cp0_status.word == 0xabcdef01u &&
        f.binding.cp0_status.known_mask == 15 &&
        f.binding.fallback_callbacks_completed + f.binding.invocations ==
            f.frame_progress.calls);
}

void persistentDirectEventsAndUnknownOldValue() {
    Nba97GameFrameInterruptDisableBinding binding{};
    binding.cp0_status = {0x80000001u, 15};
    binding.operation_budget = 2;
    for (unsigned i = 0; i < DisablePcs.size(); ++i) {
        Nba97MatchFrameCall call{DisablePcs[i], 0x80048ff4u, {0, 0}};
        Nba97GamePeriodValue value{};
        check(nba97_game_frame_interrupt_disable_from_match_frame(
            &binding, &call, &value) == NBA97_BODY_OK && value.known &&
            value.word == (i ? 0x80000000u : 0x80000001u) &&
            binding.cp0_status.word == 0x80000000u);
    }
    check(binding.invocations == 4 && binding.completions == 4);
    for (auto count : binding.call_count)
        check(count == 1);

    Fixture unknown(0xabcdef01u, 14);
    check(unknown.run() == NBA97_BODY_UNKNOWN &&
        !unknown.frame_progress.completed &&
        unknown.frame_progress.stopped_pc == 0x80049070u &&
        unknown.binding.invocations == 1 && unknown.binding.completions == 1 &&
        unknown.binding.progress[0].completed &&
        unknown.binding.progress[0].old_status.word == 0xabcdef01u &&
        unknown.binding.progress[0].old_status.known_mask == 14 &&
        unknown.binding.cp0_status.word == 0xabcdef00u &&
        unknown.binding.cp0_status.known_mask == 14 &&
        unknown.before_restore.empty());
}

void metadataFallbackAndRefusalPrefixes() {
    Nba97GameFrameInterruptDisableBinding binding{};
    binding.cp0_status = {3, 15};
    binding.operation_budget = 2;
    Nba97GamePeriodValue value{};
    Nba97MatchFrameCall good{0x80049070u, 0x80048ff4u, {0, 0}};
    auto bad = good;
    bad.pc = 0x80049074u;
    check(nba97_game_frame_interrupt_disable_from_match_frame(
        &binding, &bad, &value) == NBA97_BODY_ARGUMENT);
    bad = good;
    bad.entry = 0x8004900cu;
    check(nba97_game_frame_interrupt_disable_from_match_frame(
        &binding, &bad, &value) == NBA97_BODY_ARGUMENT);
    bad = good;
    bad.args[0] = 1;
    check(nba97_game_frame_interrupt_disable_from_match_frame(
        &binding, &bad, &value) == NBA97_BODY_ARGUMENT);
    check(nba97_game_frame_interrupt_disable_from_match_frame(
        &binding, &good, nullptr) == NBA97_BODY_ARGUMENT);

    Fixture refused;
    refused.fail_pc = 0x80049084u;
    check(refused.run() == NBA97_BODY_BOUNDS &&
        refused.binding.invocations == 1 && refused.binding.completions == 1 &&
        refused.binding.cp0_status.word == 0xabcdef00u &&
        refused.frame_progress.stopped_pc == 0x80049084u &&
        refused.before_restore.empty());

    Fixture missing;
    check(missing.run(false) == NBA97_MATCH_FRAME_IO_REQUIRED &&
        missing.binding.invocations == 0 &&
        missing.frame_progress.stopped_pc == 0x80049024u);

    Fixture limited;
    limited.binding.operation_budget = 0;
    check(limited.run() == NBA97_BODY_JOURNAL_LIMIT &&
        limited.binding.invocations == 1 && limited.binding.completions == 0 &&
        limited.binding.result[0] == NBA97_TEXT_LIMIT &&
        limited.binding.progress[0].stopped_pc == 0x80048ff4u &&
        limited.binding.cp0_status.word == 0xabcdef01u &&
        limited.frame_progress.stopped_pc == 0x80049070u);

    Fixture malformed;
    malformed.binding.cp0_status.known_mask = 16;
    check(malformed.run() == NBA97_BODY_ARGUMENT &&
        malformed.binding.invocations == 0 &&
        malformed.frame_progress.stopped_pc == 0x80049070u);

    Nba97MatchFrameProgress progress{};
    Fixture args;
    Nba97MatchFrameContext frame{Fixture::access, Fixture::io, &args, 100};
    check(nba97_game_match_frame_with_interrupt_disable(nullptr,
        &args.binding, &progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_match_frame_with_interrupt_disable(&frame, nullptr,
        &progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_match_frame_with_interrupt_disable(&frame,
        &args.binding, nullptr) == NBA97_BODY_ARGUMENT);
}
}

int main() {
    actualFrameAllFourSites();
    persistentDirectEventsAndUnknownOldValue();
    metadataFallbackAndRefusalPrefixes();
    std::printf("%u game frame interrupt-disable integration checks passed\n",
        checks);
    return 0;
}
