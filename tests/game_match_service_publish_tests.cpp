#include "recovered/game_match_service_publish.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "match service publish check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t StatusSource = 0x800f9ffeu;
constexpr std::uint32_t PhaseSource = 0x800fdb90u;
constexpr std::uint32_t StatusDestination = 0x80015028u;
constexpr std::uint32_t PhaseDestination = 0x800170bcu;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;
constexpr std::uint32_t SavedRa = EntrySp - 8u;

struct Fixture {
    enum Mode {
        Ordinary,
        MutateMachine,
        RefuseChild,
        InvalidChildMachine,
        UnknownRestoredRa,
        UnknownChildSp,
        UnalignedChildSp,
        MovedChildSp
    } mode = Ordinary;
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0x5au);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    std::array<Nba97GameMatchServicePublishAccess, 16> journal{};
    Nba97GameMatchServicePublishContext context{};
    Nba97GameMatchServicePublishProgress progress{};
    Nba97GameMatchServicePublishMachine incoming{};
    Nba97GameMatchServicePublishMachine child_entry{};
    Nba97GameMatchServicePublishEvent child_event{};
    unsigned child_calls{};

    Fixture() {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            incoming.registers.gpr[i] =
                {0x11000000u + i * 0x01010101u,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
        incoming.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        incoming.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        incoming.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x81234568u, 0x0f};
        incoming.hi = {0x13572468u, 0x05};
        incoming.lo = {0x89abcdefu, 0x0a};
        put(StatusSource, 0xabcd, 2);
        put(PhaseSource, 0xff80, 2);
        context.memory = {&region, 1};
        context.operation_budget = 7;
        context.machine = incoming;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t known_mask = 0x0f) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = static_cast<std::uint8_t>(
                (known_mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    std::uint8_t knownMask(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint8_t result = 0;
        for (unsigned i = 0; i < width; ++i)
            if (known[at + i])
                result = static_cast<std::uint8_t>(result | (1u << i));
        return result;
    }
    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameMatchServicePublishEvent* event,
        Nba97GameMatchServicePublishMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        ++f.child_calls;
        f.child_event = *event;
        f.child_entry = *machine;
        if (f.mode == RefuseChild) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                {0xdecafbad, 3};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V1] =
                {0x10203040, 5};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {0x55667788, 6};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0x800fee00, 7};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
                {0x87654321, 9};
            machine->hi = {0x31415926, 0x0a};
            machine->lo = {0x27182818, 0x0b};
            return 0;
        }
        if (f.mode == InvalidChildMachine) {
            machine->hi.known_mask = 0x10;
            return 1;
        }
        if (f.mode == UnknownRestoredRa) {
            f.known[f.offset(SavedRa) + 2u] = 0;
            return 1;
        }
        if (f.mode == UnknownChildSp) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 7;
            return 1;
        }
        if (f.mode == UnalignedChildSp) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {FrameSp + 1u, 0x0f};
            return 1;
        }
        if (f.mode == MovedChildSp) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0x800fee00u, 0x0f};
            f.put(0x800fee10u, 0x87654320u, 4);
            return 1;
        }
        if (f.mode == MutateMachine) {
            for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
                    ++i)
                machine->registers.gpr[i] = {
                    0x60000000u + i * 0x00010101u,
                    static_cast<std::uint8_t>(i & 0x0fu)};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0x800fee00u, 0x0f};
            machine->hi = {0x24681357u, 6};
            machine->lo = {0xfedcba98u, 9};
            f.put(0x800fee10u, 0x87654320u, 4);
            return 1;
        }
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            {0xaabbccddu, 5};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V1] =
            {0x10203040u, 0x0a};
        return 1;
    }
    int run() {
        context.machine = incoming;
        return nba97_game_match_service_publish(&context, &progress);
    }
};

void values_order_and_forwarding() {
    constexpr std::array<std::uint32_t, 2> statuses{{0, 0xffffu}};
    constexpr std::array<std::uint32_t, 4> phases{{
        0, 0x7fffu, 0x8000u, 0xffffu}};
    for (auto status : statuses) {
        for (auto phase : phases) {
            Fixture f;
            f.put(StatusDestination - 1u, 0x11223344u, 4);
            f.bytes[f.offset(PhaseDestination - 1u)] = 0x88;
            f.bytes[f.offset(PhaseDestination + 4u)] = 0x55;
            const auto status_left = f.bytes[f.offset(StatusDestination - 1u)];
            const auto status_right = f.bytes[f.offset(StatusDestination + 2u)];
            const auto phase_left = f.bytes[f.offset(PhaseDestination - 1u)];
            const auto phase_right = f.bytes[f.offset(PhaseDestination + 4u)];
            f.put(StatusSource, status, 2);
            f.put(PhaseSource, phase, 2);
            check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
            check(f.get(StatusDestination, 2) == status &&
                f.get(PhaseDestination, 4) ==
                    (phase & 0x8000u ? phase | 0xffff0000u : phase));
            check(f.bytes[f.offset(StatusDestination - 1u)] == status_left &&
                f.bytes[f.offset(StatusDestination + 2u)] == status_right &&
                f.bytes[f.offset(PhaseDestination - 1u)] == phase_left &&
                f.bytes[f.offset(PhaseDestination + 4u)] == phase_right);
            check(f.progress.operations == 7 && f.progress.accesses == 6 &&
                f.progress.reads == 3 && f.progress.stores == 3 &&
                f.progress.callbacks_completed == 1 &&
                f.progress.call_count[
                    NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264] == 1);
            check(f.child_event.pc == 0x8002de5cu &&
                f.child_event.delay_slot_pc == 0x8002de60u &&
                f.child_event.entry == 0x8002a264u &&
                f.child_event.operation == 6 &&
                f.child_event.invocation == 1 &&
                f.child_event.argument_count == 0);
            check(f.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                    .word == status &&
                f.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
                    (phase & 0x8000u ? phase | 0xffff0000u : phase) &&
                f.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
                    0x80010000u &&
                f.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                    FrameSp &&
                f.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                    0x8002de64u);
            for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
                    ++i) {
                if (i == NBA97_MATCH_INITIALIZE_AT ||
                    i == NBA97_MATCH_INITIALIZE_V0 ||
                    i == NBA97_MATCH_INITIALIZE_V1 ||
                    i == NBA97_MATCH_INITIALIZE_SP ||
                    i == NBA97_MATCH_INITIALIZE_RA)
                    continue;
                check(f.child_entry.registers.gpr[i].word ==
                        f.incoming.registers.gpr[i].word &&
                    f.child_entry.registers.gpr[i].known_mask ==
                        f.incoming.registers.gpr[i].known_mask &&
                    f.progress.machine.registers.gpr[i].word ==
                        f.incoming.registers.gpr[i].word &&
                    f.progress.machine.registers.gpr[i].known_mask ==
                        f.incoming.registers.gpr[i].known_mask);
            }
            check(f.child_entry.hi.word == f.incoming.hi.word &&
                f.child_entry.hi.known_mask == f.incoming.hi.known_mask &&
                f.child_entry.lo.word == f.incoming.lo.word &&
                f.child_entry.lo.known_mask == f.incoming.lo.known_mask &&
                f.progress.machine.hi.word == f.incoming.hi.word &&
                f.progress.machine.hi.known_mask == f.incoming.hi.known_mask &&
                f.progress.machine.lo.word == f.incoming.lo.word &&
                f.progress.machine.lo.known_mask == f.incoming.lo.known_mask);
            check(f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V0].word == 0xaabbccddu &&
                f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V0].known_mask == 5 &&
                f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V1].word == 0x10203040u &&
                f.progress.child_return_v1.known_mask == 0x0a);
            check(f.progress.frame_stack_pointer == FrameSp &&
                f.progress.saved_return_address.word == 0x81234568u &&
                f.progress.restored_return_address.word == 0x81234568u &&
                f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
                f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_RA].word == 0x81234568u);
            constexpr std::array<std::uint32_t, 6> pcs{{0x8002de38u,
                0x8002de40u, 0x8002de48u, 0x8002de50u,
                0x8002de58u, 0x8002de64u}};
            constexpr std::array<std::uint32_t, 6> addresses{{StatusSource,
                PhaseSource, SavedRa, StatusDestination, PhaseDestination,
                SavedRa}};
            constexpr std::array<std::uint8_t, 6> widths{{2, 2, 4, 2, 4, 4}};
            for (unsigned i = 0; i < pcs.size(); ++i)
                check(f.journal[i].pc == pcs[i] &&
                    f.journal[i].address == addresses[i] &&
                    f.journal[i].width == widths[i] &&
                    f.journal[i].operation == i + 1u + (i == 5u ? 1u : 0u));
        }
    }
}

void unknown_loads_propagate() {
    constexpr std::array<std::uint8_t, 3> status_masks{{0, 1, 2}};
    for (auto mask : status_masks) {
        Fixture f;
        f.put(StatusSource, 0xabcd, 2, mask);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.progress.loaded_status.word == 0xabcdu &&
            f.progress.loaded_status.known_mask ==
                static_cast<std::uint8_t>(mask | 0x0cu));
        check(f.get(StatusDestination, 2) == 0xabcdu &&
            f.knownMask(StatusDestination, 2) == mask);
    }
    constexpr std::array<std::uint8_t, 4> phase_masks{{0, 1, 2, 3}};
    for (auto mask : phase_masks) {
        Fixture f;
        f.put(PhaseSource, 0x80feu, 2, mask);
        check(f.run() == NBA97_TEXT_COMPLETE);
        const auto expected = static_cast<std::uint8_t>(
            (mask & 3u) | ((mask & 2u) ? 0x0cu : 0u));
        check(f.progress.loaded_phase.word == 0xffff80feu &&
            f.progress.loaded_phase.known_mask == expected);
        check(f.get(PhaseDestination, 4) == 0xffff80feu &&
            f.knownMask(PhaseDestination, 4) == expected);
    }
}

void budgets_and_refusal_prefixes() {
    constexpr std::array<std::uint32_t, 7> stopped{{0x8002de38u,
        0x8002de40u, 0x8002de48u, 0x8002de50u, 0x8002de58u,
        0x8002de5cu, 0x8002de64u}};
    constexpr std::array<unsigned, 7> reads{{0, 1, 2, 2, 2, 2, 2}};
    constexpr std::array<unsigned, 7> stores{{0, 0, 0, 1, 2, 3, 3}};
    for (unsigned budget = 0; budget < 7; ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed);
        check(f.progress.operations == budget &&
            f.progress.stopped_pc == stopped[budget] &&
            f.progress.reads == reads[budget] &&
            f.progress.stores == stores[budget]);
        check(f.child_calls == (budget == 6u ? 1u : 0u) &&
            f.progress.callbacks_completed == (budget == 6u ? 1u : 0u));
        if (budget == 5u)
            check(f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_RA].word == 0x8002de64u);
    }
    Fixture missing;
    missing.context.io = nullptr;
    check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.progress.operations == 6 && missing.progress.stores == 3 &&
        missing.progress.stopped_pc == 0x8002de5cu);
    Fixture refused;
    refused.mode = Fixture::RefuseChild;
    check(refused.run() == NBA97_TEXT_IO_REFUSED && refused.child_calls == 1 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.call_count[
            NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264] == 0 &&
        refused.progress.child_return_v0.word == 0xdecafbadu &&
        refused.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].known_mask == 5 &&
        refused.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_T0].word == 0x55667788u &&
        refused.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].known_mask == 7 &&
        refused.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x87654321u &&
        refused.progress.machine.hi.word == 0x31415926u &&
        refused.progress.machine.lo.word == 0x27182818u);
    Fixture malformed;
    malformed.mode = Fixture::InvalidChildMachine;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 6 &&
        malformed.progress.machine.hi.known_mask == 0x10);
}

void mutable_machine_and_epilogue() {
    Fixture changed;
    changed.mode = Fixture::MutateMachine;
    check(changed.run() == NBA97_TEXT_COMPLETE && changed.progress.completed);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_SP ||
            i == NBA97_MATCH_INITIALIZE_RA)
            continue;
        check(changed.progress.machine.registers.gpr[i].word ==
                0x60000000u + i * 0x00010101u &&
            changed.progress.machine.registers.gpr[i].known_mask ==
                static_cast<std::uint8_t>(i & 0x0fu));
    }
    check(changed.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x800fee18u &&
        changed.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x87654320u &&
        changed.progress.machine.hi.word == 0x24681357u &&
        changed.progress.machine.hi.known_mask == 6 &&
        changed.progress.machine.lo.word == 0xfedcba98u &&
        changed.progress.machine.lo.known_mask == 9);

    Fixture moved;
    moved.mode = Fixture::MovedChildSp;
    check(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.restored_return_address.word == 0x87654320u &&
        moved.journal[5].address == 0x800fee10u);
    Fixture unknown_sp;
    unknown_sp.mode = Fixture::UnknownChildSp;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.operations == 6 &&
        unknown_sp.progress.stopped_pc == 0x8002de64u);
    Fixture unaligned_sp;
    unaligned_sp.mode = Fixture::UnalignedChildSp;
    check(unaligned_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_sp.progress.operations == 7 &&
        unaligned_sp.progress.stopped_address == SavedRa + 1u);
    Fixture unknown_ra;
    unknown_ra.mode = Fixture::UnknownRestoredRa;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 7 &&
        unknown_ra.progress.stopped_pc == 0x8002de6cu &&
        unknown_ra.progress.restored_return_address.known_mask == 0x0bu &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp);

    Fixture incoming_sp;
    incoming_sp.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {EntrySp, 7};
    check(incoming_sp.run() == NBA97_TEXT_UNKNOWN &&
        incoming_sp.progress.operations == 2 &&
        incoming_sp.progress.stopped_pc == 0x8002de48u &&
        incoming_sp.progress.stores == 0);
}

int simple_child(void*, const Nba97GameTextMemory*,
    const Nba97GameMatchServicePublishEvent*,
    Nba97GameMatchServicePublishMachine*) {
    return 1;
}

void mapping_alias_and_metadata_cases() {
    Fixture unmapped;
    unmapped.region.size = 0x100u;
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x8002de38u &&
        unmapped.progress.operations == 1);

    Fixture missing_stack;
    std::array<Nba97GameTextRegion, 4> partial{{
        {StatusSource, missing_stack.bytes.data() + 0x10,
            missing_stack.known.data() + 0x10, 2},
        {PhaseSource, missing_stack.bytes.data() + 0x20,
            missing_stack.known.data() + 0x20, 2},
        {StatusDestination, missing_stack.bytes.data() + 0x30,
            missing_stack.known.data() + 0x30, 2},
        {PhaseDestination, missing_stack.bytes.data() + 0x40,
            missing_stack.known.data() + 0x40, 4}}};
    partial[0].data[0] = 0x34;
    partial[0].data[1] = 0x12;
    partial[1].data[0] = 0x78;
    partial[1].data[1] = 0x56;
    missing_stack.context.memory = {partial.data(), partial.size()};
    check(missing_stack.run() == NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc == 0x8002de48u &&
        missing_stack.progress.operations == 3);

    Fixture missing_status_destination;
    std::array<Nba97GameTextRegion, 4> no_status_destination{{
        {StatusSource, missing_status_destination.bytes.data() + 0x10,
            missing_status_destination.known.data() + 0x10, 2},
        {PhaseSource, missing_status_destination.bytes.data() + 0x20,
            missing_status_destination.known.data() + 0x20, 2},
        {SavedRa, missing_status_destination.bytes.data() + 0x30,
            missing_status_destination.known.data() + 0x30, 4},
        {PhaseDestination, missing_status_destination.bytes.data() + 0x40,
            missing_status_destination.known.data() + 0x40, 4}}};
    no_status_destination[0].data[0] = 0x34;
    no_status_destination[0].data[1] = 0x12;
    no_status_destination[1].data[0] = 0x78;
    no_status_destination[1].data[1] = 0x56;
    missing_status_destination.context.memory = {
        no_status_destination.data(), no_status_destination.size()};
    check(missing_status_destination.run() == NBA97_TEXT_RESOURCE &&
        missing_status_destination.progress.stopped_pc == 0x8002de50u &&
        missing_status_destination.progress.operations == 4 &&
        missing_status_destination.progress.stores == 1);

    Fixture missing_phase_destination;
    std::array<Nba97GameTextRegion, 4> no_phase_destination{{
        {StatusSource, missing_phase_destination.bytes.data() + 0x10,
            missing_phase_destination.known.data() + 0x10, 2},
        {PhaseSource, missing_phase_destination.bytes.data() + 0x20,
            missing_phase_destination.known.data() + 0x20, 2},
        {SavedRa, missing_phase_destination.bytes.data() + 0x30,
            missing_phase_destination.known.data() + 0x30, 4},
        {StatusDestination, missing_phase_destination.bytes.data() + 0x40,
            missing_phase_destination.known.data() + 0x40, 2}}};
    no_phase_destination[0].data[0] = 0x34;
    no_phase_destination[0].data[1] = 0x12;
    no_phase_destination[1].data[0] = 0x78;
    no_phase_destination[1].data[1] = 0x56;
    missing_phase_destination.context.memory = {
        no_phase_destination.data(), no_phase_destination.size()};
    check(missing_phase_destination.run() == NBA97_TEXT_RESOURCE &&
        missing_phase_destination.progress.stopped_pc == 0x8002de58u &&
        missing_phase_destination.progress.operations == 5 &&
        missing_phase_destination.progress.stores == 2);

    Fixture malformed;
    malformed.known[malformed.offset(StatusSource)] = 2;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1);
    Fixture bad_machine;
    bad_machine.incoming.registers.gpr[0] = {1, 0x0f};
    check(bad_machine.run() == NBA97_TEXT_ARGUMENT &&
        bad_machine.progress.operations == 0);
    Fixture overlaps;
    std::array<Nba97GameTextRegion, 2> overlap{{
        {Ram, overlaps.bytes.data(), overlaps.known.data(), 8},
        {Ram + 4u, overlaps.bytes.data() + 8,
            overlaps.known.data() + 8, 8}}};
    overlaps.context.memory = {overlap.data(), overlap.size()};
    check(overlaps.run() == NBA97_TEXT_ARGUMENT);
    Fixture region_wrap;
    Nba97GameTextRegion wrapping_region{
        0xfffffff0u, region_wrap.bytes.data(), region_wrap.known.data(), 32};
    region_wrap.context.memory = {&wrapping_region, 1};
    check(region_wrap.run() == NBA97_TEXT_ARGUMENT);

    Fixture wrapped;
    wrapped.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 0x0f};
    std::array<std::uint8_t, 32> low_data{};
    std::array<std::uint8_t, 32> low_known{};
    low_known.fill(1);
    std::array<Nba97GameTextRegion, 2> wrap_regions{{
        {0, low_data.data(), low_known.data(), low_data.size()},
        {Ram, wrapped.bytes.data(), wrapped.known.data(), wrapped.bytes.size()}}};
    wrapped.context.memory = {wrap_regions.data(), wrap_regions.size()};
    check(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapped.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 8u &&
        wrapped.progress.restored_return_address.word == 0x81234568u);

    std::array<std::uint8_t, 64> alias_data{};
    std::array<std::uint8_t, 64> alias_known{};
    alias_known.fill(1);
    alias_data[0] = 0xcd;
    alias_data[1] = 0xab;
    alias_data[8] = 0x80;
    alias_data[9] = 0xff;
    std::array<Nba97GameTextRegion, 5> read_alias{{
        {StatusSource, alias_data.data(), alias_known.data(), 2},
        {PhaseSource, alias_data.data() + 8, alias_known.data() + 8, 2},
        {SavedRa, alias_data.data(), alias_known.data(), 4},
        {StatusDestination, alias_data.data() + 16,
            alias_known.data() + 16, 2},
        {PhaseDestination, alias_data.data() + 24,
            alias_known.data() + 24, 4}}};
    Nba97GameMatchServicePublishContext alias_context{};
    Nba97GameMatchServicePublishProgress alias_progress{};
    alias_context.memory = {read_alias.data(), read_alias.size()};
    alias_context.operation_budget = 7;
    alias_context.machine = Fixture{}.incoming;
    alias_context.io = simple_child;
    check(nba97_game_match_service_publish(&alias_context, &alias_progress) ==
            NBA97_TEXT_COMPLETE &&
        alias_data[16] == 0xcd && alias_data[17] == 0xab &&
        alias_progress.loaded_status.word == 0xabcdu);

    std::array<std::uint8_t, 64> publish_alias_data{};
    std::array<std::uint8_t, 64> publish_alias_known{};
    publish_alias_known.fill(1);
    publish_alias_data[0] = 0x12;
    publish_alias_data[1] = 0x34;
    publish_alias_data[8] = 0x34;
    publish_alias_data[9] = 0x12;
    std::array<Nba97GameTextRegion, 5> publish_alias{{
        {StatusSource, publish_alias_data.data(), publish_alias_known.data(), 2},
        {PhaseSource, publish_alias_data.data() + 8,
            publish_alias_known.data() + 8, 2},
        {SavedRa, publish_alias_data.data() + 24,
            publish_alias_known.data() + 24, 4},
        {StatusDestination, publish_alias_data.data() + 16,
            publish_alias_known.data() + 16, 2},
        {PhaseDestination, publish_alias_data.data() + 24,
            publish_alias_known.data() + 24, 4}}};
    Nba97GameMatchServicePublishContext publish_context{};
    Nba97GameMatchServicePublishProgress publish_progress{};
    publish_context.memory = {publish_alias.data(), publish_alias.size()};
    publish_context.operation_budget = 7;
    publish_context.machine = Fixture{}.incoming;
    publish_context.io = simple_child;
    check(nba97_game_match_service_publish(&publish_context,
            &publish_progress) == NBA97_TEXT_COMPLETE &&
        publish_progress.restored_return_address.word == 0x1234u &&
        publish_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x1234u);
}

void unknown_store_without_bitmap_and_native_misalignment() {
    Fixture f;
    std::array<std::uint8_t, 128> data{};
    std::array<std::uint8_t, 128> known{};
    known.fill(1);
    data[1] = 0x34;
    data[2] = 0x12;
    known[1] = 0;
    data[17] = 0x78;
    data[18] = 0x56;
    std::array<Nba97GameTextRegion, 5> regions{{
        {StatusSource, data.data() + 1, known.data() + 1, 2},
        {PhaseSource, data.data() + 17, known.data() + 17, 2},
        {SavedRa, data.data() + 33, known.data() + 33, 4},
        {StatusDestination, data.data() + 49, nullptr, 2},
        {PhaseDestination, data.data() + 65, known.data() + 65, 4}}};
    f.context.memory = {regions.data(), regions.size()};
    check(f.run() == NBA97_TEXT_ARGUMENT &&
        f.progress.operations == 4 && f.progress.stores == 1 &&
        f.progress.stopped_pc == 0x8002de50u &&
        data[49] == 0 && data[50] == 0);

    known[1] = 1;
    check(f.run() == NBA97_TEXT_COMPLETE && data[49] == 0x34 &&
        data[50] == 0x12 && data[65] == 0x78 && data[66] == 0x56);
}
}

int main() {
    values_order_and_forwarding();
    unknown_loads_propagate();
    budgets_and_refusal_prefixes();
    mutable_machine_and_epilogue();
    mapping_alias_and_metadata_cases();
    unknown_store_without_bitmap_and_native_misalignment();
    std::printf("game match service publish: %u checks passed\n", checks);
    return 0;
}
