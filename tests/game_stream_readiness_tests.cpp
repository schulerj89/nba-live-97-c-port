#include "recovered/game_stream_readiness.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "stream readiness check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Flag = 0x800f0fdcu;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t Frame = EntrySp - 0x18u;

bool sameWord(const Nba97GameStreamReadinessWord& a,
    const Nba97GameStreamReadinessWord& b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0xcd);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameStreamReadinessContext context{};
    Nba97GameStreamReadinessProgress progress{};
    std::array<Nba97GameStreamReadinessAccess, 8> journal{};
    Nba97GameStreamReadinessEvent event{};
    Nba97GameStreamReadinessMachine child_entry{};
    Nba97GameStreamReadinessWord child_v0{1, 15};
    int accepted = 1;
    bool mutate = false;
    bool malformed = false;
    std::uint32_t alternate_frame = 0x800fe000u;
    unsigned calls = 0;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 20;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] = {
                0x31000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.machine.registers.gpr[0] = {0, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
            {0xabcdef01u, 5};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8002a2f4u, 15};
        context.machine.hi = {0x12345678u, 3};
        context.machine.lo = {0x9abcdef0u, 12};
        put(Flag, 1, 2, 3);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t mask = 15) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    std::uint8_t getKnown(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < width; ++i)
            mask = static_cast<std::uint8_t>(mask | (known[at + i] << i));
        return mask;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameStreamReadinessEvent* event,
        Nba97GameStreamReadinessMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        ++f.calls;
        f.event = *event;
        f.child_entry = *machine;
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = f.child_v0;
        if (f.mutate) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {0xfeedbeefu, 9};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_A3] =
                {0x13572468u, 6};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0xdead0000u, 7};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
                {f.alternate_frame, 15};
            machine->hi = {0x0badc0deu, 5};
            machine->lo = {0xc001d00du, 10};
            f.put(f.alternate_frame + 0x14u, 0x800448c0u, 4, 15);
            f.put(f.alternate_frame + 0x10u, 0x76543210u, 4, 7);
        }
        if (f.malformed)
            machine->registers.gpr[0] = {1, 15};
        return f.accepted;
    }
    int run() {
        return nba97_game_stream_readiness(&context, &progress);
    }
};

void flagAndSignedChildDomain() {
    for (std::uint32_t flag : {0u, 1u, 0xffffu, 0x8000u}) {
        const std::array<std::int32_t, 6> values{{
            std::numeric_limits<std::int32_t>::min(), -1, 0, 1, 2,
            std::numeric_limits<std::int32_t>::max()}};
        for (auto child : values) {
            Fixture f;
            f.put(Flag, flag, 2, 3);
            f.child_v0 = {static_cast<std::uint32_t>(child), 15};
            const auto original = f.context.machine;
            check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
            const bool enabled = static_cast<std::int16_t>(flag) != 0;
            const auto expected = enabled && child < 2 ? 1u : 0u;
            check(f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V0].word == expected &&
                f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V0].known_mask == 15);
            check(f.calls == (enabled ? 1u : 0u) &&
                f.progress.operations == (enabled ? 6u : 5u) &&
                f.progress.stores == 2 && f.progress.reads == 3 &&
                f.progress.access_events == 5);
            check(f.progress.frame_stack_pointer == Frame &&
                f.progress.loaded_flag.word ==
                    static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(
                            static_cast<std::int16_t>(flag))) &&
                f.progress.loaded_flag.known_mask == 15);
            check(f.get(Frame + 0x14u, 4) == 0x8002a2f4u &&
                f.getKnown(Frame + 0x14u, 4) == 15 &&
                f.get(Frame + 0x10u, 4) == 0xabcdef01u &&
                f.getKnown(Frame + 0x10u, 4) == 5);
            check(sameWord(f.progress.restored_return_address,
                    original.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
                sameWord(f.progress.restored_s8,
                    original.registers.gpr[NBA97_MATCH_INITIALIZE_FP]) &&
                f.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
                sameWord(f.progress.machine.hi, original.hi) &&
                sameWord(f.progress.machine.lo, original.lo));
            if (enabled) {
                check(f.event.pc == 0x80088d30u &&
                    f.event.delay_slot_pc == 0x80088d34u &&
                    f.event.entry == 0x80084448u &&
                    f.event.operation == 4 && f.event.invocation == 1 &&
                    f.event.argument_count == 0 &&
                    f.child_entry.registers.gpr[
                        NBA97_MATCH_INITIALIZE_RA].word == 0x80088d38u &&
                    f.child_entry.registers.gpr[
                        NBA97_MATCH_INITIALIZE_RA].known_mask == 15);
            }
            check(f.journal[0].pc == 0x80088d10u &&
                f.journal[0].address == Frame + 0x14u &&
                f.journal[0].kind == NBA97_GAME_MATCH_CLOCKS_STORE &&
                f.journal[1].pc == 0x80088d14u &&
                f.journal[2].pc == 0x80088d20u &&
                f.journal[2].address == Flag &&
                f.journal[2].width == 2 &&
                f.journal[3].pc == 0x80088d68u &&
                f.journal[4].pc == 0x80088d6cu);
        }
    }
}

void mutableMachineFrameAndAliases() {
    Fixture f;
    f.mutate = true;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 1);
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            0xfeedbeefu &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask ==
            9 &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
            0x13572468u &&
        f.progress.machine.hi.word == 0x0badc0deu &&
        f.progress.machine.lo.word == 0xc001d00du);
    check(f.progress.restored_return_address.word == 0x800448c0u &&
        f.progress.restored_s8.word == 0x76543210u &&
        f.progress.restored_s8.known_mask == 7 &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            f.alternate_frame + 0x18u &&
        f.journal[3].address == f.alternate_frame + 0x14u &&
        f.journal[4].address == f.alternate_frame + 0x10u);

    /* Entry sp=flag+4 makes the first spill alias the later flag LH. */
    Fixture alias;
    alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Flag + 4u, 15};
    alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x80000001u, 15};
    alias.put(Flag, 0, 2, 3);
    alias.child_v0 = {2, 15};
    check(alias.run() == NBA97_TEXT_COMPLETE && alias.calls == 1 &&
        alias.progress.loaded_flag.word == 1 &&
        alias.journal[0].address == Flag &&
        alias.journal[2].address == Flag &&
        alias.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80000001u);
}

void knownnessAndFailurePrefixes() {
    Fixture flag_unknown;
    flag_unknown.put(Flag, 0, 2, 2); /* known zero high byte only */
    check(flag_unknown.run() == NBA97_TEXT_UNKNOWN &&
        flag_unknown.calls == 0 && flag_unknown.progress.operations == 3 &&
        flag_unknown.progress.stopped_pc == 0x80088d28u &&
        flag_unknown.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e);

    Fixture predicate_unknown;
    predicate_unknown.child_v0 = {1, 8}; /* positive sign, unknown magnitude */
    check(predicate_unknown.run() == NBA97_TEXT_UNKNOWN &&
        predicate_unknown.progress.operations == 4 &&
        predicate_unknown.progress.stopped_pc == 0x80088d3cu &&
        predicate_unknown.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].word == 1 &&
        predicate_unknown.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].known_mask == 0x0e);

    Fixture negative_known_sign;
    negative_known_sign.child_v0 = {0x80000000u, 8};
    check(negative_known_sign.run() == NBA97_TEXT_COMPLETE &&
        negative_known_sign.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 1);

    Fixture refused;
    refused.accepted = 0;
    check(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls == 1 &&
        refused.progress.operations == 4 &&
        refused.progress.stopped_pc == 0x80088d30u &&
        refused.progress.stopped_entry == 0x80084448u &&
        refused.progress.callbacks_completed == 0);

    Fixture missing;
    missing.context.io = nullptr;
    check(missing.run() == NBA97_TEXT_IO_REFUSED && missing.calls == 0 &&
        missing.progress.operations == 4 &&
        missing.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80088d38u);

    Fixture malformed;
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.calls == 1 &&
        malformed.progress.callbacks_completed == 0 &&
        malformed.progress.machine.registers.gpr[0].word == 1);

    Fixture unknown_ra;
    unknown_ra.put(Flag, 0, 2, 3);
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800448c0u, 7};
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 5 &&
        unknown_ra.progress.stopped_pc == 0x80088d74u &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp);

    Fixture unknown_sp;
    unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
        .known_mask = 7;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.operations == 0 &&
        unknown_sp.progress.stopped_pc == 0x80088d10u);

    Fixture mutable_s8_unknown;
    mutable_s8_unknown.mutate = true;
    mutable_s8_unknown.alternate_frame = 0x800fe000u;
    /* The callback's normal mutation is replaced by making its chosen s8
     * partially known through a dedicated callback result below. */
    mutable_s8_unknown.malformed = false;
    mutable_s8_unknown.context.user = &mutable_s8_unknown;
    /* Exercise unknown live s8 by using a known callback then changing the
     * value through a one-off callback wrapper. */
    mutable_s8_unknown.context.io = [](void* user,
        const Nba97GameTextMemory*, const Nba97GameStreamReadinessEvent*,
        Nba97GameStreamReadinessMachine* machine) -> int {
        auto& x = *static_cast<Fixture*>(user);
        ++x.calls;
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {1, 15};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
            {0x800fe000u, 7};
        return 1;
    };
    check(mutable_s8_unknown.run() == NBA97_TEXT_UNKNOWN &&
        mutable_s8_unknown.progress.operations == 4 &&
        mutable_s8_unknown.progress.stopped_pc == 0x80088d68u);
}

void budgetsMappingAlignmentWrapAndMetadata() {
    const std::array<std::uint32_t, 6> enabled_stops{{
        0x80088d10u, 0x80088d14u, 0x80088d20u,
        0x80088d30u, 0x80088d68u, 0x80088d6cu}};
    for (std::size_t budget = 0; budget < enabled_stops.size(); ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == enabled_stops[budget] &&
            f.progress.access_events == (budget < 3 ? budget :
                (budget < 4 ? 3u : budget - 1u)));
    }
    Fixture enabled_complete;
    enabled_complete.context.operation_budget = 6;
    check(enabled_complete.run() == NBA97_TEXT_COMPLETE);

    const std::array<std::uint32_t, 5> disabled_stops{{
        0x80088d10u, 0x80088d14u, 0x80088d20u,
        0x80088d68u, 0x80088d6cu}};
    for (std::size_t budget = 0; budget < disabled_stops.size(); ++budget) {
        Fixture f;
        f.put(Flag, 0, 2, 3);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.stopped_pc == disabled_stops[budget] &&
            f.progress.operations == budget && f.calls == 0);
    }

    Fixture unaligned;
    unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 1;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 1 && unaligned.progress.accesses == 1 &&
        unaligned.progress.stopped_pc == 0x80088d10u);

    Fixture unmapped;
    unmapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x90000000u, 15};
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.operations == 1 && unmapped.progress.stores == 0);

    Fixture no_bitmap;
    no_bitmap.region.known = nullptr;
    no_bitmap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 7;
    check(no_bitmap.run() == NBA97_TEXT_ARGUMENT &&
        no_bitmap.progress.operations == 1 && no_bitmap.progress.stores == 0);

    Fixture bad_byte;
    bad_byte.known[bad_byte.offset(Frame + 0x14u)] = 2;
    check(bad_byte.run() == NBA97_TEXT_ARGUMENT &&
        bad_byte.progress.operations == 1 && bad_byte.progress.accesses == 1);

    Fixture invalid_zero;
    invalid_zero.context.machine.registers.gpr[0] = {1, 15};
    check(invalid_zero.run() == NBA97_TEXT_ARGUMENT &&
        invalid_zero.progress.operations == 0);
    Fixture invalid_mask;
    invalid_mask.context.machine.hi.known_mask = 16;
    check(invalid_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture missing_journal;
    missing_journal.context.access_journal = nullptr;
    check(missing_journal.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    std::array<Nba97GameTextRegion, 2> regions{{overlap.region, overlap.region}};
    overlap.context.memory = {regions.data(), regions.size()};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);

    /* Wrapping address arithmetic: entry sp=0x18 produces frame zero,
     * preserving the wrapped uint32_t address domain without host pointers. */
    Fixture wrapped;
    std::array<std::uint8_t, 0x20> low{};
    std::array<std::uint8_t, 0x20> low_known{};
    low_known.fill(1);
    std::array<Nba97GameTextRegion, 2> wrap_regions{{
        {0, low.data(), low_known.data(), low.size()}, wrapped.region}};
    wrapped.context.memory = {wrap_regions.data(), wrap_regions.size()};
    wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x18, 15};
    wrapped.put(Flag, 0, 2, 3);
    check(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0 &&
        wrapped.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x18u);
}
}

int main() {
    flagAndSignedChildDomain();
    mutableMachineFrameAndAliases();
    knownnessAndFailurePrefixes();
    budgetsMappingAlignmentWrapAndMetadata();
    std::printf("%u game stream readiness checks passed\n", checks);
    return 0;
}
