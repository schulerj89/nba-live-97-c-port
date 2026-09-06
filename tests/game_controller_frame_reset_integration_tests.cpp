#include "game_controller_frame_reset_adapter.h"

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
        std::fprintf(stderr,
            "game controller-frame reset integration check %u failed\n",
            checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t Table = 0x800fdc50u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    std::array<Nba97GameControllerFrameResetAccess, 24> journal{};
    Nba97GameControllerFrameResetTickBinding binding{};
    Nba97MatchTickContext tick{};
    Nba97MatchTickProgress tick_progress{};
    std::vector<Nba97MatchTickCall> fallback_calls;
    Nba97GameControllerFrameResetEvent child_event{};
    Nba97GameControllerFrameResetRegisters child_registers{};
    bool refuse_child{};
    bool stop_after_reset{true};

    Composition() {
        binding.memory = {&region, 1};
        binding.entry_context_source_proven = 1;
        binding.operation_budget = 64;
        binding.io = childIo;
        binding.user = this;
        binding.access_journal = journal.data();
        binding.access_journal_capacity = journal.size();
        binding.fallback_service = fallback;
        binding.fallback_user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            binding.entry_registers.gpr[i] = {
                0x52000000u + i * 0x00010101u, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
            EntrySp, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x80068cfcu, 0x0f};
        put(0x8001edecu, 0, 2);
        put(0x800fdb92u, 2, 2);
        put(0x800fdb8au, 0, 2);
        put(0x800fdb6cu, 1, 2);
        put(0x800fe90eu, 5, 2);
        for (unsigned i = 0; i < 8; ++i) {
            const std::uint32_t pointer = 0x80002000u + i * 0x100u;
            put(Table + i * 4u, pointer, 4);
            put(pointer + 0x28u, 0xbe00u + i, 2);
        }
        tick.access = access;
        tick.service = nba97_game_controller_frame_reset_from_match_tick;
        tick.user = &binding;
        tick.operation_budget = 64;
        tick.incoming_s6 = {0, 0};
    }

    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        const auto offset = static_cast<std::size_t>(address - Ram);
        for (unsigned i = 0; i < width; ++i) {
            bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[offset + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto offset = static_cast<std::size_t>(address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[offset + i]) << (8u * i);
        return value;
    }
    int run() { return nba97_game_match_tick(&tick, &tick_progress); }

    static int access(void* opaque, std::uint32_t pc,
        std::uint32_t address, unsigned width, unsigned kind,
        Nba97PlayerFrameValue* value) {
        auto& binding =
            *static_cast<Nba97GameControllerFrameResetTickBinding*>(opaque);
        auto& c = *static_cast<Composition*>(binding.fallback_user);
        if (c.stop_after_reset && pc == 0x80068cfcu)
            return NBA97_BODY_BOUNDS;
        if (!value || (width != 1 && width != 2 && width != 4) ||
            address < Ram ||
            std::uint64_t(address - Ram) + width > c.bytes.size())
            return NBA97_BODY_BOUNDS;
        const auto offset = static_cast<std::size_t>(address - Ram);
        if (kind == NBA97_FRAME_READ) {
            value->word = 0;
            value->known_mask = 0;
            value->is_reference = 0;
            value->reference = {};
            for (unsigned i = 0; i < width; ++i) {
                value->word |= std::uint32_t(c.bytes[offset + i]) << (8u * i);
                value->known_mask = static_cast<std::uint8_t>(
                    value->known_mask | (c.known[offset + i] << i));
            }
        } else {
            for (unsigned i = 0; i < width; ++i) {
                c.bytes[offset + i] = static_cast<std::uint8_t>(
                    value->word >> (8u * i));
                c.known[offset + i] = static_cast<std::uint8_t>(
                    (value->known_mask >> i) & 1u);
            }
        }
        return NBA97_BODY_OK;
    }
    static int fallback(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue*) {
        auto& c = *static_cast<Composition*>(user);
        c.fallback_calls.push_back(*call);
        const bool allowed =
            (call->pc == 0x80068c24u && call->entry == 0x80066f88u) ||
            (call->pc == 0x80068c2cu && call->entry == 0x80079664u) ||
            (call->pc == 0x80068c4cu && call->entry == 0x80067468u) ||
            (call->pc == 0x80068cecu && call->entry == 0x80067550u);
        return allowed ? NBA97_BODY_OK : NBA97_MATCH_TICK_SERVICE_REQUIRED;
    }
    static int childIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameControllerFrameResetEvent* event,
        Nba97GameControllerFrameResetRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.child_event = *event;
        c.child_registers = *registers;
        if (c.refuse_child)
            return 0;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 5};
        return 1;
    }
};

void natural_match_tick_composition() {
    Composition c;
    check(c.run() == NBA97_BODY_BOUNDS);
    check(c.tick_progress.operations == 15 &&
        c.tick_progress.services == 5 && !c.tick_progress.completed &&
        c.tick_progress.stopped_pc == 0x80068cfcu &&
        c.tick_progress.stopped_address == 0x800fdb7cu);
    check(c.fallback_calls.size() == 4);
    const std::array<std::uint32_t, 4> pcs{{
        0x80068c24u, 0x80068c2cu, 0x80068c4cu, 0x80068cecu
    }};
    for (std::size_t i = 0; i < pcs.size(); ++i)
        check(c.fallback_calls[i].pc == pcs[i]);
    check(c.binding.invocations == 1 &&
        c.binding.result == NBA97_TEXT_COMPLETE &&
        c.binding.progress.completed &&
        c.binding.progress.operations == 22 &&
        c.binding.progress.callbacks_completed == 1);
    check(c.child_event.pc == 0x8006764cu &&
        c.child_event.delay_slot_pc == 0x80067650u &&
        c.child_event.entry == 0x80083eecu &&
        c.child_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067654u &&
        c.child_registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 8 &&
        c.child_registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            Table + 0x20u);
    check(c.get(0x800fe90eu, 2) == 3 &&
        c.get(0x800fdb92u, 2) == 0 &&
        c.get(0x800fdb6cu, 2) == 2 &&
        c.get(0x800fdb6eu, 2) == 32);
    for (unsigned i = 0; i < 8; ++i)
        check(c.get(0x80002028u + i * 0x100u, 2) == 0);
    check(c.binding.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        0xcafebabeu &&
        c.binding.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .known_mask == 5 &&
        c.binding.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        c.binding.progress.restored_return_address.word == 0x80068cfcu);
}

void nested_refusal_and_budget_prefix() {
    Composition refused;
    refused.refuse_child = true;
    check(refused.run() ==
        NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE);
    check(refused.tick_progress.operations == 14 &&
        refused.tick_progress.services == 4 &&
        refused.tick_progress.stopped_pc == 0x80068cf4u &&
        refused.tick_progress.stopped_entry == 0x800675e4u);
    check(refused.binding.invocations == 1 &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.operations == 21 &&
        refused.binding.progress.stopped_pc == 0x8006764cu &&
        refused.binding.progress.stopped_entry == 0x80083eecu);

    Composition limited;
    limited.binding.operation_budget = 4;
    check(limited.run() == NBA97_BODY_JOURNAL_LIMIT &&
        limited.tick_progress.operations == 14 &&
        limited.tick_progress.services == 4 &&
        limited.binding.result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.operations == 4 &&
        limited.binding.progress.stopped_pc == 0x80067634u &&
        limited.binding.progress.controller_slots_cleared == 0);
}

void explicit_entry_context_and_adapter_guards() {
    Composition absent;
    absent.binding.entry_context_source_proven = 0;
    check(absent.run() ==
        NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CONTEXT_REQUIRED);
    check(absent.tick_progress.operations == 14 &&
        absent.tick_progress.services == 4 &&
        absent.binding.invocations == 0 &&
        absent.binding.result == NBA97_TEXT_ARGUMENT);

    Composition malformed;
    Nba97MatchTickCall call{0x80068cf4u, 0x800675e4u, {0, 0}, 1};
    check(nba97_game_controller_frame_reset_from_match_tick(
        &malformed.binding, &call, nullptr) ==
        NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE);
    check(malformed.binding.invocations == 0 &&
        malformed.binding.result == NBA97_TEXT_ARGUMENT);
    call.count = 0;
    Nba97GamePeriodValue unexpected{};
    check(nba97_game_controller_frame_reset_from_match_tick(
        &malformed.binding, &call, &unexpected) ==
        NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE);
    call.pc ^= 4u;
    check(nba97_game_controller_frame_reset_from_match_tick(
        &malformed.binding, &call, nullptr) ==
        NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE);
    check(nba97_game_controller_frame_reset_from_match_tick(
        nullptr, &call, nullptr) == NBA97_BODY_ARGUMENT);

    Nba97MatchTickCall prior{0x80068cecu, 0x80067550u, {0, 0}, 0};
    check(nba97_game_controller_frame_reset_from_match_tick(
        &malformed.binding, &prior, nullptr) == NBA97_BODY_OK &&
        malformed.fallback_calls.size() == 1);
}
}

int main() {
    natural_match_tick_composition();
    nested_refusal_and_budget_prefix();
    explicit_entry_context_and_adapter_guards();
    std::printf("game controller-frame reset integration: %u checks passed\n",
        checks);
}
