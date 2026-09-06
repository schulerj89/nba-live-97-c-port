#include "game_frame_interrupt_restore_adapter.h"
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
            "game frame interrupt-restore integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

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

void actualFrameDisableRestoreComposition() {
    Fixture f;
    check(f.run() == NBA97_BODY_OK && f.frame_progress.completed &&
        f.disable.invocations == 13 && f.disable.completions == 13 &&
        f.restore.invocations == 13 && f.restore.completions == 13 &&
        f.after_disable.size() == 13);
    const std::array<std::size_t, 4> expected_counts{{1, 10, 1, 1}};
    for (unsigned i = 0; i < 4; ++i) {
        const auto& disabled = f.disable.progress[i];
        const auto& restored = f.restore.progress[i];
        check(f.disable.event[i].pc == DisablePcs[i] &&
            f.restore.event[i].pc == RestorePcs[i] &&
            f.restore.event[i].entry == 0x8004900cu &&
            f.restore.event[i].args[0] == 0xabcdef01u &&
            f.restore.event[i].args[1] == 0 &&
            f.disable.call_count[i] == expected_counts[i] &&
            f.restore.call_count[i] == expected_counts[i]);
        check(disabled.old_status.word == 0xabcdef01u &&
            disabled.new_status.word == 0xabcdef00u &&
            restored.published_status.word == 0xabcdef01u &&
            restored.published_status.known_mask == 15 &&
            restored.machine.cp0_status.word == 0xabcdef01u);
        check(restored.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_A0].word == 0xabcdef01u &&
            restored.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_A0].known_mask == 15 &&
            restored.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].word == RestorePcs[i] + 8u &&
            restored.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].known_mask == 15 &&
            restored.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V0].known_mask == 0);
        for (unsigned r = 1; r < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++r)
            if (r != NBA97_MATCH_INITIALIZE_A0 &&
                r != NBA97_MATCH_INITIALIZE_RA)
                check(restored.machine.registers.gpr[r].known_mask == 0);
    }
    for (const auto& status : f.after_disable)
        check(status.word == 0xabcdef00u && status.known_mask == 15);
    check(f.restore.cp0_status.word == 0xabcdef01u &&
        f.restore.cp0_status.known_mask == 15 &&
        f.restore.fallback_callbacks_completed + f.restore.invocations ==
            f.frame_progress.calls);
}

void restoreBudgetMetadataAndFallbackPrefixes() {
    Fixture limited;
    limited.restore.operation_budget = 0;
    check(limited.run() == NBA97_BODY_JOURNAL_LIMIT &&
        limited.disable.invocations == 1 && limited.disable.completions == 1 &&
        limited.restore.invocations == 1 && limited.restore.completions == 0 &&
        limited.restore.result[0] == NBA97_TEXT_LIMIT &&
        limited.restore.progress[0].stopped_pc == 0x8004900cu &&
        limited.restore.cp0_status.word == 0xabcdef00u &&
        limited.frame_progress.stopped_pc == 0x8004909cu);

    Fixture refused;
    refused.fail_pc = 0x800490acu;
    check(refused.run() == NBA97_BODY_BOUNDS &&
        refused.disable.invocations == 1 && refused.restore.invocations == 1 &&
        refused.restore.cp0_status.word == 0xabcdef01u &&
        refused.frame_progress.stopped_pc == 0x800490acu);

    Fixture missing;
    check(missing.run(false) == NBA97_MATCH_FRAME_IO_REQUIRED &&
        missing.disable.invocations == 0 && missing.restore.invocations == 0 &&
        missing.frame_progress.stopped_pc == 0x80049024u);

    Nba97GameFrameInterruptRestoreBinding binding{};
    binding.cp0_status = {7, 15};
    binding.operation_budget = 1;
    Nba97GamePeriodValue value{};
    Nba97MatchFrameCall good{0x8004909cu, 0x8004900cu,
        {0x12345678u, 0}};
    auto bad = good;
    bad.pc = 0x800490a0u;
    check(nba97_game_frame_interrupt_restore_from_match_frame(
        &binding, &bad, &value) == NBA97_BODY_ARGUMENT);
    bad = good;
    bad.entry = 0x80048ff4u;
    check(nba97_game_frame_interrupt_restore_from_match_frame(
        &binding, &bad, &value) == NBA97_BODY_ARGUMENT);
    bad = good;
    bad.args[1] = 1;
    check(nba97_game_frame_interrupt_restore_from_match_frame(
        &binding, &bad, &value) == NBA97_BODY_ARGUMENT);
    check(nba97_game_frame_interrupt_restore_from_match_frame(
        &binding, &good, nullptr) == NBA97_BODY_ARGUMENT);
    binding.cp0_status.known_mask = 16;
    check(nba97_game_frame_interrupt_restore_from_match_frame(
        &binding, &good, &value) == NBA97_BODY_ARGUMENT);

    Nba97MatchFrameProgress progress{};
    Fixture args;
    Nba97MatchFrameContext frame{Fixture::access, Fixture::io, &args, 100};
    check(nba97_game_match_frame_with_interrupt_restore(nullptr,
        &args.restore, &progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_match_frame_with_interrupt_restore(&frame, nullptr,
        &progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_match_frame_with_interrupt_restore(&frame,
        &args.restore, nullptr) == NBA97_BODY_ARGUMENT);
}
}

int main() {
    actualFrameDisableRestoreComposition();
    restoreBudgetMetadataAndFallbackPrefixes();
    std::printf("%u game frame interrupt-restore integration checks passed\n",
        checks);
    return 0;
}
