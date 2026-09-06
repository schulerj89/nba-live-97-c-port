#include "game_clear_ordering_table_adapter.h"

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
            "clear-ordering-table integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)
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
        check(call->pc == (invocation == 1 ? 0x80049084u : 0x80049094u));
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

void actualFrameComposition() {
    Fixture f;
    check(f.run() == NBA97_BODY_OK && f.frame_progress.completed &&
        f.provider_calls == 2 && f.binding.invocations == 2 &&
        f.binding.completions == 2 && f.owner_calls.size() == 2);
    const std::array<U32, 2> pcs{{0x80049084u, 0x80049094u}};
    const std::array<U32, 2> objects{{0x800fccf0u, 0x800f5c50u}};
    const std::array<U32, 2> counts{{32u, 4096u}};
    for (unsigned i = 0; i < 2; ++i) {
        const auto& event = f.binding.event[i];
        const auto& progress = f.binding.progress[i];
        const auto& child = f.owner_entries[i];
        check(event.pc == pcs[i] && event.entry == 0x80099960u &&
            event.args[0] == objects[i] && event.args[1] == counts[i] &&
            f.binding.call_count[i] == 1 &&
            f.binding.result[i] == NBA97_TEXT_COMPLETE && progress.completed);
        check(child.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                objects[i] &&
            child.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                counts[i] &&
            child.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x800999c4u &&
            child.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
                U32(0x10000000u * (i + 1u) +
                    NBA97_MATCH_INITIALIZE_T0));
        check(progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_SP].word ==
                U32(0x80180000u + (i + 1u) * 0x100u) &&
            progress.machine.hi.word == 0xabc00000u + i + 1u &&
            progress.machine.lo.word == 0xdef00000u + i + 1u &&
            progress.return_v0.word == objects[i] &&
            progress.restored_return_address.word == pcs[i] + 8u &&
            f.get(objects[i]) == 0x000c567cu);
        check(f.owner_calls[i].kind ==
                NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND &&
            f.owner_calls[i].target == 0x8009a97cu &&
            f.owner_calls[i].argument_count == 2 &&
            f.owner_calls[i].pc == 0x800999bcu);
    }
    check(f.binding.fallback_callbacks_completed +
        f.binding.invocations == f.frame_progress.calls);
    for (const auto& call : f.fallback_calls)
        check(call.entry != 0x80099960u);
}

void directMetadataProviderAndFallback() {
    Fixture f;
    Nba97GamePeriodValue value{0xffffffffu, 1};
    Nba97MatchFrameCall first{0x80049084u, 0x80099960u,
        {0x800fccf0u, 32u}};
    check(nba97_game_clear_ordering_table_from_match_frame(
        &f.binding, &first, &value) == NBA97_BODY_OK &&
        !value.known && value.word == 0 && f.provider_calls == 1);

    Fixture missing;
    missing.refuse_provider = true;
    check(nba97_game_clear_ordering_table_from_match_frame(
        &missing.binding, &first, &value) ==
        NBA97_GAME_CLEAR_ORDERING_TABLE_ENTRY_MACHINE_REQUIRED &&
        missing.binding.invocations == 1 && missing.binding.completions == 0);

    Fixture bad;
    auto call = first;
    call.entry = 0x80099964u;
    check(nba97_game_clear_ordering_table_from_match_frame(
        &bad.binding, &call, &value) ==
        NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE);
    call = first;
    call.args[1] = 1;
    check(nba97_game_clear_ordering_table_from_match_frame(
        &bad.binding, &call, &value) ==
        NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE);
    call = first;
    call.pc = 0x80049088u;
    check(nba97_game_clear_ordering_table_from_match_frame(
        &bad.binding, &call, &value) ==
        NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE);

    Nba97MatchFrameCall other{0x80049024u, 0x800530fcu, {0, 0}};
    check(nba97_game_clear_ordering_table_from_match_frame(
        &bad.binding, &other, &value) == NBA97_MATCH_FRAME_IO_REQUIRED);
    bad.binding.fallback = Fixture::fallback;
    bad.binding.fallback_user = &bad;
    check(nba97_game_clear_ordering_table_from_match_frame(
        &bad.binding, &other, &value) == NBA97_BODY_OK &&
        bad.fallback_calls.size() == 1);
}

void nestedRefusalAndArguments() {
    Fixture refused;
    refused.refuse_backend = true;
    check(refused.run() ==
            NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE &&
        !refused.frame_progress.completed &&
        refused.frame_progress.stopped_pc == 0x80049084u &&
        refused.binding.invocations == 1 &&
        refused.binding.progress[0].stopped_pc == 0x800999bcu &&
        refused.get(0x800fccf0u) == 0);

    Fixture limited;
    limited.binding.operation_budget = 0;
    check(limited.run() == NBA97_BODY_JOURNAL_LIMIT &&
        limited.frame_progress.stopped_pc == 0x80049084u &&
        limited.binding.result[0] == NBA97_TEXT_LIMIT);

    Fixture args;
    Nba97MatchFrameContext frame{Fixture::access, Fixture::fallback,
        &args, 100};
    Nba97MatchFrameProgress progress{};
    check(nba97_game_match_frame_with_clear_ordering_table(nullptr,
        &args.binding, &progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_match_frame_with_clear_ordering_table(&frame,
        nullptr, &progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_match_frame_with_clear_ordering_table(&frame,
        &args.binding, nullptr) == NBA97_BODY_ARGUMENT);
    check(nba97_game_clear_ordering_table_from_match_frame(
        nullptr, nullptr, nullptr) == NBA97_BODY_ARGUMENT);
}
}

int main() {
    actualFrameComposition();
    directMetadataProviderAndFallback();
    nestedRefusalAndArguments();
    std::printf("%u clear-ordering-table integration checks passed\n", checks);
    return 0;
}
