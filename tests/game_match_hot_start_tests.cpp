#include "recovered/game_match_hot_start.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
constexpr std::uint32_t Base = 0x80000000u;
constexpr std::size_t Size = 0x120000u;
constexpr std::uint32_t Stack = 0x80010000u;
constexpr std::uint32_t LeftTable = 0x8001ec98u;
constexpr std::uint32_t RightTable = 0x800170c8u;
constexpr std::uint32_t PrefixTable = 0x800fe920u;
constexpr std::uint32_t Root = 0x80050000u;
constexpr std::uint32_t HotPayload = 0x80060000u;
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
    Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
    std::vector<Nba97GameMatchHotStartAccess> journal =
        std::vector<Nba97GameMatchHotStartAccess>(1024);
    Nba97GameMatchHotStartContext context{};
    Nba97GameMatchHotStartProgress progress{};
    std::vector<std::uint32_t> loader_results{0x80070000u};
    std::vector<Nba97GameMatchHotStartEvent> calls;
    std::size_t loader_index = 0;
    std::size_t alias_observations = 0;
    bool always_zero = false;
    bool refuse = false;
    std::size_t refuse_call = 0;
    bool corrupt_zero = false;
    bool mutate_live = false;
    bool unknown_loader = false;
    bool relocate_stack = false;
    bool partial_saved_ra = false;
    std::uint32_t final_v0 = 0xabcdef01u;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 2000;
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234560u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x10203040u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1] =
            {0x50607080u, 0x0f};
        context.io = &Fixture::io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put32(0x80020becu, Root);
        put32(Root, 0x80071000u);
        put32(Root + 0x20u, HotPayload);
        put8(HotPayload + 9u, 0x2au);
        for (unsigned i = 0; i < 84; ++i) {
            put32(LeftTable + i * 4u, 0);
            put32(RightTable + i * 4u, 0);
        }
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Base);
    }
    void put8(std::uint32_t address, std::uint8_t value,
        std::uint8_t is_known = 1) {
        bytes[offset(address)] = value;
        known[offset(address)] = is_known;
    }
    void put16(std::uint32_t address, std::uint16_t value,
        std::uint8_t known_mask = 3) {
        for (unsigned i = 0; i < 2; ++i) {
            bytes[offset(address) + i] = static_cast<std::uint8_t>(value >> (8 * i));
            known[offset(address) + i] =
                static_cast<std::uint8_t>((known_mask >> i) & 1u);
        }
    }
    void put32(std::uint32_t address, std::uint32_t value,
        std::uint8_t known_mask = 0x0f) {
        for (unsigned i = 0; i < 4; ++i) {
            bytes[offset(address) + i] = static_cast<std::uint8_t>(value >> (8 * i));
            known[offset(address) + i] =
                static_cast<std::uint8_t>((known_mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[offset(address) + i]) << (8 * i);
        return value;
    }
    std::uint8_t knownMask(std::uint32_t address, unsigned width) const {
        std::uint8_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value = static_cast<std::uint8_t>(value |
                (known[offset(address) + i] << i));
        return value;
    }
    int run() { return nba97_game_match_hot_start(&context, &progress); }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchHotStartEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        const std::size_t call_number = f.calls.size();
        if (f.refuse && call_number == f.refuse_call)
            return 0;
        if (event->entry == 0x80051ed8u) {
            if (event->pc == 0x80067034u) {
                check(event->delay_slot_pc == 0x80067038u,
                    "first child delay-slot PC");
                check(registers->gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x80071000u,
                    "first child receives *s0");
                check(registers->gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x4eu,
                    "first child receives 0x4e");
                check(registers->gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8006703cu,
                    "first child sees JAL ra");
                check(registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word == 1,
                    "first child sees s1 delay assignment");
                if (f.mutate_live) {
                    constexpr std::uint32_t AlternateRoot = 0x80052000u;
                    registers->gpr[NBA97_MATCH_INITIALIZE_S0] =
                        {AlternateRoot, 0x0f};
                    f.put32(AlternateRoot, 0x80072000u);
                    f.put32(AlternateRoot + 0x20u, HotPayload + 0x100u);
                    f.put8(HotPayload + 0x109u, 0xf3u);
                }
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x11111111u, 0x0f};
            } else {
                check(event->pc == 0x80067088u &&
                    event->delay_slot_pc == 0x8006708cu,
                    "final child call and NOP delay");
                const std::uint32_t expected_a0 =
                    f.mutate_live ? 0x80072000u : 0x80071000u;
                const std::uint32_t expected_a1 = f.mutate_live ? 0xf3u : 0x2au;
                check(registers->gpr[NBA97_MATCH_INITIALIZE_A0].word == expected_a0,
                    "final child receives live *s0");
                check(registers->gpr[NBA97_MATCH_INITIALIZE_A1].word == expected_a1,
                    "final child receives unsigned payload byte");
                if (f.relocate_stack) {
                    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {0x80011000u, 0x0f};
                    f.put32(0x80011018u, 0x82345678u,
                        f.partial_saved_ra ? 0x07 : 0x0f);
                    f.put32(0x80011014u, 0x13579bdfu);
                    f.put32(0x80011010u, 0x2468ace0u);
                }
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {f.final_v0, 0x0f};
            }
        } else {
            check(event->entry == 0x800a72bcu && event->pc == 0x80067054u,
                "loader exact boundary");
            check(event->delay_slot_pc == 0x80067058u,
                "loader NOP delay PC");
            check(registers->gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x800275b8u &&
                registers->gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x800c6400u,
                "loader receives zhots source arguments");
            if (f.corrupt_zero)
                registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
            if (f.mutate_live)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] =
                    {0x76543210u + static_cast<std::uint32_t>(f.loader_index), 0x0f};
            if (f.unknown_loader)
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x7fu, 0x0e};
            else if (f.always_zero)
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
            else {
                const std::size_t index = std::min(f.loader_index,
                    f.loader_results.size() - 1);
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                    {f.loader_results[index], 0x0f};
            }
            ++f.loader_index;
        }
        return 1;
    }
};

void normalPrefixesAndOrder() {
    Fixture f;
    std::uint32_t sum = 0;
    std::vector<std::uint16_t> expected(84);
    f.context.observer = [](void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchHotStartAccess* access,
        Nba97GameMatchInitializeRegisters* registers) -> int {
        auto& fixture = *static_cast<Fixture*>(user);
        if (access->pc == 0x80066fccu) {
            check(registers->gpr[NBA97_MATCH_INITIALIZE_V0].word == access->value,
                "observer sees post-LBU v0 while event retains pre-load EA");
            ++fixture.alias_observations;
        } else if (access->pc == 0x80067024u) {
            check(registers->gpr[NBA97_MATCH_INITIALIZE_S0].word == Root,
                "observer sees post-LW s0 while event retains pre-load EA");
            ++fixture.alias_observations;
        }
        return 1;
    };
    for (unsigned i = 0; i < 84; ++i) {
        const std::uint32_t left = 0x80030000u + i * 0x20u;
        const std::uint32_t right = 0x80040000u + i * 0x20u;
        const std::uint8_t lv = static_cast<std::uint8_t>((i * 17u) & 0xffu);
        const std::uint8_t rv = static_cast<std::uint8_t>((255u - i * 3u) & 0xffu);
        expected[i] = static_cast<std::uint16_t>(sum);
        if (i % 5u != 0) {
            f.put32(LeftTable + i * 4u, left);
            f.put8(left + 7u, lv);
        }
        if (i % 7u != 0) {
            f.put32(RightTable + i * 4u, right);
            f.put8(right + 7u, rv);
        }
        const unsigned actual_left = i % 5u ? lv : 0;
        const unsigned actual_right = i % 7u ? rv : 0;
        sum += std::max(actual_left, actual_right);
    }
    check(f.run() == NBA97_TEXT_COMPLETE, "normal run completes");
    check(f.progress.completed && f.progress.prefixes_written == 84,
        "all 84 prefixes complete");
    for (unsigned i = 0; i < 84; ++i)
        check(f.get(PrefixTable + i * 2u, 2) == expected[i],
            "each prefix stores pre-add low halfword");
    check(f.get(0x800fe91cu, 4) == 0x80070000u &&
        f.get(0x800d7af8u, 4) == 1 && f.get(0x8002148cu, 2) == 0,
        "loader pointer, ready flag, and final clear published");
    check(f.progress.retry_attempts == 1 && f.calls.size() == 3,
        "one loader attempt between two initializer calls");
    check(f.alias_observations == 68,
        "all aliased address/target reads expose correct observer ordering");
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == f.final_v0,
        "final child v0 is returned");
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x81234560u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Stack,
        "saved ra and original sp restored");
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word == 0x80020000u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x80071000u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x2au &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
            LeftTable + 84u * 4u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
            RightTable + 84u * 4u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word == 84u &&
        f.progress.registers.gpr[9].word == sum,
        "final source scratch registers match the 72-instruction path");
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const bool source_writes = i == NBA97_MATCH_INITIALIZE_ZERO ||
            i == NBA97_MATCH_INITIALIZE_AT ||
            i == NBA97_MATCH_INITIALIZE_V0 || i == NBA97_MATCH_INITIALIZE_V1 ||
            (i >= NBA97_MATCH_INITIALIZE_A0 && i <= NBA97_MATCH_INITIALIZE_A3) ||
            i == NBA97_MATCH_INITIALIZE_T0 || i == 9 ||
            i == NBA97_MATCH_INITIALIZE_S0 || i == NBA97_MATCH_INITIALIZE_S0 + 1 ||
            i == NBA97_MATCH_INITIALIZE_SP || i == NBA97_MATCH_INITIALIZE_RA;
        if (!source_writes)
            check(f.progress.registers.gpr[i].word == 0x11000000u + i &&
                f.progress.registers.gpr[i].known_mask == 0x0f,
                "all untouched GPRs remain byte-exact");
    }

    std::size_t cursor = 3;
    for (unsigned i = 0; i < 84; ++i) {
        check(f.journal[cursor].pc == 0x80066fb8u &&
            f.journal[cursor].address == PrefixTable + i * 2u,
            "prefix access order");
        ++cursor;
        check(f.journal[cursor].pc == 0x80066fbcu &&
            f.journal[cursor].address == LeftTable + i * 4u,
            "left table read order");
        ++cursor;
        if (i % 5u != 0) {
            check(f.journal[cursor].pc == 0x80066fccu &&
                f.journal[cursor].address ==
                    0x80030000u + i * 0x20u + 7u,
                "aliased v0 base/target journal retains left payload EA");
            ++cursor;
        }
        check(f.journal[cursor].pc == 0x80066fdcu &&
            f.journal[cursor].address == RightTable + i * 4u,
            "right table read order");
        ++cursor;
        if (i % 7u != 0) {
            check(f.journal[cursor].pc == 0x80066fecu,
                "right payload follows right pointer");
            ++cursor;
        }
    }
    check(f.progress.access_events > cursor &&
        f.journal[cursor].pc == 0x80067024u &&
        f.journal[cursor].address == 0x80020becu &&
        f.journal[cursor].value == Root,
        "aliased s0 base/target journal retains root EA and loaded value");
    const std::size_t end = f.progress.access_events;
    check(end >= 3 && f.journal[end - 3].pc == 0x80067090u &&
        f.journal[end - 2].pc == 0x80067094u &&
        f.journal[end - 1].pc == 0x80067098u,
        "ra, s1, and s0 reload in exact final stack order");
    check(f.calls[0].kind == NBA97_MATCH_HOT_START_CHILD_80051ED8 &&
        f.calls[1].kind == NBA97_MATCH_HOT_START_CHILD_800A72BC &&
        f.calls[2].kind == NBA97_MATCH_HOT_START_CHILD_80051ED8 &&
        f.calls[0].argument_count == 2 && f.calls[1].argument_count == 2 &&
        f.calls[2].argument_count == 2,
        "typed call kinds and argument counts preserve source order");
}

void tiesUnsignedAndWrap() {
    Fixture f;
    const std::uint32_t left = 0x80030000u;
    const std::uint32_t right = 0x80040000u;
    f.put32(LeftTable, left);
    f.put32(RightTable, right);
    f.put8(left + 7u, 255);
    f.put8(right + 7u, 255);
    f.context.observer = [](void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchHotStartAccess* access,
        Nba97GameMatchInitializeRegisters* registers) -> int {
        (void)user;
        if (access->pc == 0x80066fb8u && access->address == PrefixTable)
            registers->gpr[9] = {0xfffffff0u, 0x0f};
        return 1;
    };
    check(f.run() == NBA97_TEXT_COMPLETE, "wrap fixture completes");
    check(f.get(PrefixTable, 2) == 0 && f.get(PrefixTable + 2u, 2) == 0x00efu,
        "observer-live t1 wraps and SH truncates");
    check(f.progress.registers.gpr[9].word == 0x000000efu,
        "84 unsigned maxima use 32-bit wrapping addu");
}

void retriesAndLiveMutation() {
    Fixture f;
    f.loader_results = {0, 0, 0x80073330u};
    f.mutate_live = true;
    f.relocate_stack = true;
    check(f.run() == NBA97_TEXT_COMPLETE, "retry and mutation run completes");
    check(f.progress.retry_attempts == 3 && f.calls.size() == 5,
        "two zero results retry before success");
    check(f.get(0x800fe91cu, 4) == 0x80073330u &&
        f.get(0x800d7af8u, 4) == 0x76543212u,
        "raw successful v0 and live s1 are published");
    check(f.progress.restored_return_address.word == 0x82345678u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x82345678u,
        "final callback-relocated ra reload is live");
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
        0x13579bdfu &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 0x2468ace0u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x80011020u,
        "relocated saved s1/s0 and sp restore in source order");
}

void unknownAndFailurePrefixes() {
    {
        Fixture f;
        f.put32(LeftTable, 0, 0x0e);
        check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80066fc4u,
            "unknown null decision stops at consuming branch");
    }
    {
        Fixture f;
        f.put32(LeftTable, 0x80030000u);
        f.put8(0x80030007u, 77, 0);
        check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x80066ff8u &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e,
            "unknown LBU propagates through SLT to its branch");
    }
    {
        Fixture f;
        f.unknown_loader = true;
        check(f.run() == NBA97_TEXT_UNKNOWN && f.progress.stopped_pc == 0x8006706cu,
            "unknown raw loader v0 stops only at retry branch");
        check(f.get(0x800fe91cu, 4) == 0x7fu &&
            f.knownMask(0x800fe91cu, 4) == 0x0e,
            "unknown raw loader v0 is stored byte-for-byte first");
        check(f.get(0x800d7af8u, 4) == 1,
            "ready flag write survives unknown retry decision");
    }
    {
        Fixture f;
        f.relocate_stack = true;
        f.partial_saved_ra = true;
        check(f.run() == NBA97_TEXT_UNKNOWN &&
            f.progress.stopped_pc == 0x800670a0u &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                0x80011020u &&
            f.progress.restored_return_address.known_mask == 0x07,
            "unknown saved ra propagates through restore until JR consumes it");
    }
    for (std::size_t refused = 1; refused <= 3; ++refused) {
        Fixture f;
        f.refuse = true;
        f.refuse_call = refused;
        const int result = f.run();
        check(result == NBA97_TEXT_IO_REFUSED,
            "each distinct child refusal preserves prefix");
        check(f.progress.callbacks_completed == refused - 1,
            "only completed callbacks counted before refusal");
    }
    {
        Fixture f;
        f.corrupt_zero = true;
        check(f.run() == NBA97_TEXT_ARGUMENT,
            "malformed callback GPR state is rejected");
    }
    {
        Fixture f;
        f.context.observer = [](void*, const Nba97GameTextMemory*,
            const Nba97GameMatchHotStartAccess*,
            Nba97GameMatchInitializeRegisters*) -> int { return 0; };
        check(f.run() == NBA97_TEXT_IO_REFUSED && f.progress.operations == 1 &&
            f.progress.stores == 1,
            "observer refusal retains completed first stack store");
    }
}

void boundedRetryAndAllBudgets() {
    Fixture full;
    check(full.run() == NBA97_TEXT_COMPLETE, "budget baseline completes");
    const std::size_t complete_operations = full.progress.operations;
    for (std::size_t budget = 0; budget < complete_operations; ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT,
            "every operation budget exposes a bounded prefix");
        check(f.progress.operations == budget && !f.progress.completed,
            "budget prefix consumes exactly its allowance");
    }
    Fixture persistent;
    persistent.always_zero = true;
    persistent.context.operation_budget = complete_operations + 10;
    check(persistent.run() == NBA97_TEXT_LIMIT &&
        persistent.progress.retry_attempts >= 2,
        "persistent source retry is bounded only by operation budget");
    bool saw_ready = false;
    for (std::size_t i = 0; i < persistent.progress.access_events; ++i)
        if (persistent.journal[i].pc == 0x80067068u &&
            persistent.journal[i].value == 1)
            saw_ready = true;
    check(saw_ready, "bounded persistent retry preserves ready-flag writes");
}

void delaySlotsSurviveBudgetStops() {
    {
        Fixture f;
        f.context.operation_budget = 257;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.stopped_pc == 0x80067034u &&
            f.progress.stopped_entry == 0x80051ed8u &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x8006703cu &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word == 1,
            "first JAL and s1 delay slot precede a denied child budget");
    }
    {
        Fixture f;
        f.context.operation_budget = 259;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.stopped_pc == 0x80067054u &&
            f.progress.stopped_entry == 0x800a72bcu &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x8006705cu,
            "loader JAL assigns ra before a denied child budget");
    }
    {
        Fixture f;
        f.context.operation_budget = 266;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.stopped_pc == 0x80067088u &&
            f.progress.stopped_entry == 0x80051ed8u &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x80067090u,
            "final JAL assigns ra before a denied child budget");
    }
}

void addressMappingAlignmentAndArguments() {
    {
        Fixture f;
        f.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word = 0x80010001u;
        check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
            f.progress.stopped_pc == 0x80066facu,
            "unaligned source stack traps on first SW");
    }
    {
        Fixture f;
        f.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word = 0x10u;
        check(f.run() == NBA97_TEXT_RESOURCE &&
            f.progress.frame_stack_pointer == 0xfffffff0u &&
            f.progress.stopped_address == 8u,
            "stack and effective address arithmetic wraps at 32 bits");
    }
    {
        Fixture f;
        f.put32(0x80020becu, 0x80050001u);
        check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
            f.progress.stopped_pc == 0x8006702cu,
            "unaligned live s0 dereference traps");
    }
    {
        Fixture f;
        f.put32(LeftTable, 0xfffffff9u);
        check(f.run() == NBA97_TEXT_RESOURCE &&
            f.progress.stopped_pc == 0x80066fccu &&
            f.progress.stopped_address == 0,
            "payload effective address wraps before mapping");
    }
    {
        Fixture f;
        f.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
        check(f.run() == NBA97_TEXT_UNKNOWN &&
            f.progress.stopped_pc == 0x80066facu,
            "unknown stack arithmetic propagates to first access");
    }
    {
        Fixture f;
        f.known[f.offset(Stack - 8u)] = 2;
        check(f.run() == NBA97_TEXT_ARGUMENT,
            "noncanonical reached memory knownness is rejected");
    }
    {
        Fixture f;
        Nba97GameTextRegion duplicate = f.region;
        Nba97GameTextRegion regions[2]{f.region, duplicate};
        f.context.memory = {regions, 2};
        check(f.run() == NBA97_TEXT_ARGUMENT,
            "overlapping source mappings are rejected");
    }
    {
        Fixture f;
        f.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
        check(f.run() == NBA97_TEXT_ARGUMENT,
            "malformed hardwired zero register is rejected");
    }
    {
        std::uint8_t shared[4]{0, 0, 0, 0};
        std::uint8_t known[4]{1, 1, 1, 1};
        Nba97GameTextRegion regions[2]{{Stack - 8u, shared, known, 4},
            {0x80020000u, shared, known, 4}};
        Nba97GameMatchHotStartContext context{};
        Nba97GameMatchHotStartProgress progress{};
        context.memory = {regions, 2};
        context.operation_budget = 1;
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x11223344u, 0x0f};
        check(nba97_game_match_hot_start(&context, &progress) == NBA97_TEXT_LIMIT &&
            shared[0] == 0x44 && progress.stores == 1,
            "nonoverlapping source regions may alias native storage");
    }
}
}

int main() {
    normalPrefixesAndOrder();
    tiesUnsignedAndWrap();
    retriesAndLiveMutation();
    unknownAndFailurePrefixes();
    boundedRetryAndAllBudgets();
    delaySlotsSurviveBudgetStops();
    addressMappingAlignmentAndArguments();
    if (failures) {
        std::cerr << failures << " match hot-start checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "match hot-start source semantics verified\n";
    return EXIT_SUCCESS;
}
