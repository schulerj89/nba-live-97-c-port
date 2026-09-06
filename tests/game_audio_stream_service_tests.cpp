#include "recovered/game_audio_stream_service.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game audio stream service check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Globals = 0x80104000u;
constexpr std::uint32_t HeaderPointer = 0x8010473cu;
constexpr std::uint32_t Header = 0x80200000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t Frame = EntrySp - 0x18u;
constexpr std::uint32_t CallerRa = 0x80083f80u;

struct Fixture {
    std::array<std::uint8_t, 0x1000> globals{};
    std::array<std::uint8_t, 0x1000> globalsKnown{};
    std::array<std::uint8_t, 0x100> header{};
    std::array<std::uint8_t, 0x100> headerKnown{};
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stackKnown{};
    std::array<Nba97GameTextRegion, 3> regions{{
        {Globals, globals.data(), globalsKnown.data(), globals.size()},
        {Header, header.data(), headerKnown.data(), header.size()},
        {Stack, stack.data(), stackKnown.data(), stack.size()}}};
    std::array<Nba97GameAudioStreamServiceAccess, 16> journal{};
    Nba97GameAudioStreamServiceContext context{};
    Nba97GameAudioStreamServiceProgress progress{};
    std::vector<Nba97GameAudioStreamServiceEvent> events;
    bool refuse = false;
    bool malformed = false;
    bool mutateAll = false;
    std::uint32_t alternateFrame = Stack + 0x100u;
    std::uint32_t childV0 = 0xdeadbeefu;

    explicit Fixture(std::uint32_t state = 1) {
        globalsKnown.fill(1);
        headerKnown.fill(1);
        stackKnown.fill(1);
        stack.fill(0xcd);
        put32(HeaderPointer, Header);
        put32(Header + 0x24u, state);
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 32;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x21000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_FP] = {0xa1b2c3d4u, 5};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 15};
        context.io = callback;
        context.user = this;
    }

    std::uint8_t* data(std::uint32_t address) {
        if (address >= Stack)
            return stack.data() + (address - Stack);
        if (address >= Header)
            return header.data() + (address - Header);
        return globals.data() + (address - Globals);
    }
    std::uint8_t* known(std::uint32_t address) {
        if (address >= Stack)
            return stackKnown.data() + (address - Stack);
        if (address >= Header)
            return headerKnown.data() + (address - Header);
        return globalsKnown.data() + (address - Globals);
    }
    void put32(std::uint32_t address, std::uint32_t value,
        std::uint8_t mask = 15) {
        for (unsigned i = 0; i < 4; ++i) {
            data(address)[i] = static_cast<std::uint8_t>(value >> (8u * i));
            known(address)[i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get32(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(data(address)[i]) << (8u * i);
        return value;
    }
    std::uint8_t mask32(std::uint32_t address) {
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < 4; ++i)
            mask = static_cast<std::uint8_t>(mask |
                (known(address)[i] ? 1u << i : 0u));
        return mask;
    }
    static int callback(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioStreamServiceEvent* event,
        Nba97GameAudioStreamServiceRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.events.push_back(*event);
        if (f.mutateAll) {
            for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
                registers->gpr[i] = {0x41000000u + i * 0x00010101u,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {0x81234560u, 15};
            registers->gpr[NBA97_MATCH_INITIALIZE_FP] =
                {f.alternateFrame, 15};
            f.put32(f.alternateFrame + 0x14u, 0x8002cbe8u, 15);
            f.put32(f.alternateFrame + 0x10u, 0x10203040u, 10);
        }
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {f.childV0, 11};
        if (f.malformed)
            registers->gpr[NBA97_MATCH_INITIALIZE_A3].known_mask = 16;
        return f.refuse ? 0 : 1;
    }
    int run() {
        return nba97_game_audio_stream_service(&context, &progress);
    }
};

void stateAndJournalCoverage() {
    for (std::uint32_t state : {0u, 1u, 2u, 0xffffffffu, 0x80000000u}) {
        Fixture f(state);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(f.progress.global_pointer.word == Header &&
            f.progress.global_pointer.known_mask == 15 &&
            f.progress.header_state.word == state &&
            f.progress.header_state.known_mask == 15);
        check(f.progress.operations == (state == 1 ? 6u : 7u) &&
            f.progress.accesses == 6 && f.progress.reads == 4 &&
            f.progress.stores == 2);
        check(f.events.size() == (state == 1 ? 0u : 1u) &&
            f.progress.callbacks_completed == (state == 1 ? 0u : 1u));
        check(f.progress.returned_value.word ==
            (state == 1 ? 1u : f.childV0));
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp && f.progress.restored_return_address.word == CallerRa &&
            f.progress.restored_s8.word == 0xa1b2c3d4u &&
            f.progress.restored_s8.known_mask == 5);
        const std::array<std::uint32_t, 6> pcs{{0x80086194u, 0x80086198u,
            0x800861a4u, 0x800861acu, 0x800861d0u, 0x800861d4u}};
        const std::array<std::uint32_t, 6> addresses{{Frame + 0x14u,
            Frame + 0x10u, HeaderPointer, Header + 0x24u,
            Frame + 0x14u, Frame + 0x10u}};
        for (unsigned i = 0; i < pcs.size(); ++i)
            check(f.journal[i].pc == pcs[i] &&
                f.journal[i].address == addresses[i] &&
                f.journal[i].operation == i + 1u + (i >= 4 && state != 1) &&
                f.journal[i].width == 4 &&
                f.journal[i].kind == (i < 2 ?
                    NBA97_GAME_AUDIO_STREAM_SERVICE_STORE :
                    NBA97_GAME_AUDIO_STREAM_SERVICE_READ));
        if (state != 1)
            check(f.events[0].pc == 0x800861c4u &&
                f.events[0].delay_slot_pc == 0x800861c8u &&
                f.events[0].entry == 0x800861e4u &&
                f.events[0].operation == 5 &&
                f.events[0].invocation == 1 &&
                f.events[0].argument_count == 0);
    }
}

void fullGprMutationAndAliases() {
    Fixture f(0);
    const auto entry = f.context.registers;
    f.mutateAll = true;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_SP ||
            i == NBA97_MATCH_INITIALIZE_FP ||
            i == NBA97_MATCH_INITIALIZE_RA ||
            i == NBA97_MATCH_INITIALIZE_V0)
            continue;
        check(f.progress.registers.gpr[i].word ==
            0x41000000u + i * 0x00010101u);
        check(f.progress.registers.gpr[i].known_mask == (i % 15u) + 1u);
    }
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        f.childV0 && f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 11);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        f.alternateFrame + 0x18u);
    check(f.progress.restored_return_address.word == 0x8002cbe8u &&
        f.progress.restored_return_address.known_mask == 15 &&
        f.progress.restored_s8.word == 0x10203040u &&
        f.progress.restored_s8.known_mask == 10);
    check(f.get32(Frame + 0x14u) == CallerRa &&
        f.get32(Frame + 0x10u) == entry.gpr[NBA97_MATCH_INITIALIZE_FP].word);

    /* pointer+0x24 aliases the just-saved ra word, proving the state load is
     * live and follows both frame stores. */
    Fixture alias(2);
    alias.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {1, 15};
    alias.put32(HeaderPointer, Frame - 0x10u);
    alias.regions[1] = {Frame - 0x20u, alias.stack.data() +
        (Frame - 0x20u - Stack), alias.stackKnown.data() +
        (Frame - 0x20u - Stack), 0x80};
    alias.context.memory.count = 2;
    check(alias.run() == NBA97_TEXT_COMPLETE && alias.events.empty() &&
        alias.progress.header_state.word == 1 &&
        alias.progress.returned_value.word == 1);

    /* A stack frame may occupy the global region. The first SW then replaces
     * the later global pointer load, preserving source access order. */
    Fixture globalAlias(2);
    globalAlias.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {HeaderPointer + 4u, 15};
    globalAlias.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Header, 15};
    globalAlias.put32(Header + 0x24u, 1);
    check(globalAlias.run() == NBA97_TEXT_COMPLETE &&
        globalAlias.events.empty() &&
        globalAlias.progress.global_pointer.word == Header &&
        globalAlias.progress.header_state.word == 1 &&
        globalAlias.journal[0].address == HeaderPointer);
}

void pointerMappingAlignmentWrapAndKnownness() {
    /* Null is unchecked and works when guest address 0x24 is mapped. */
    Fixture nullPointer(2);
    std::array<std::uint8_t, 0x40> low{};
    std::array<std::uint8_t, 0x40> lowKnown{};
    lowKnown.fill(1);
    low[0x24] = 1;
    nullPointer.put32(HeaderPointer, 0);
    nullPointer.regions[1] = {0, low.data(), lowKnown.data(), low.size()};
    check(nullPointer.run() == NBA97_TEXT_COMPLETE &&
        nullPointer.events.empty() &&
        nullPointer.progress.header_state.word == 1);

    Fixture wrapped(2);
    low.fill(0);
    low[0] = 2;
    wrapped.put32(HeaderPointer, 0xffffffdcu);
    wrapped.regions[1] = {0, low.data(), lowKnown.data(), low.size()};
    check(wrapped.run() == NBA97_TEXT_COMPLETE && wrapped.events.size() == 1 &&
        wrapped.journal[3].address == 0);

    Fixture unaligned(2);
    unaligned.put32(HeaderPointer, Header + 1u);
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 4 &&
        unaligned.progress.accesses == 4 &&
        unaligned.progress.stopped_pc == 0x800861acu &&
        unaligned.progress.stopped_address == Header + 0x25u);

    Fixture missing(2);
    missing.regions[1] = missing.regions[2];
    missing.context.memory.count = 2;
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.operations == 4 &&
        missing.progress.stopped_pc == 0x800861acu &&
        missing.progress.stopped_address == Header + 0x24u);

    Fixture unknownPointer(2);
    unknownPointer.put32(HeaderPointer, Header, 14);
    check(unknownPointer.run() == NBA97_TEXT_UNKNOWN &&
        unknownPointer.progress.operations == 3 &&
        unknownPointer.progress.stopped_pc == 0x800861acu);

    Fixture provenUnequal(2);
    provenUnequal.put32(Header + 0x24u, 2, 1);
    check(provenUnequal.run() == NBA97_TEXT_COMPLETE &&
        provenUnequal.events.size() == 1);

    Fixture unknownState(1);
    unknownState.put32(Header + 0x24u, 1, 14);
    check(unknownState.run() == NBA97_TEXT_UNKNOWN &&
        unknownState.progress.operations == 4 &&
        unknownState.progress.stopped_pc == 0x800861b4u &&
        unknownState.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 1 &&
        unknownState.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 15);

    Fixture unknownSp(1);
    unknownSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 14;
    check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.operations == 0 &&
        unknownSp.progress.stopped_pc == 0x80086194u);

    Fixture unalignedSp(1);
    unalignedSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 1u;
    check(unalignedSp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedSp.progress.operations == 1 &&
        unalignedSp.progress.stopped_address == Frame + 1u + 0x14u);

    Fixture unknownRa(1);
    unknownRa.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
    check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.operations == 6 &&
        unknownRa.progress.stopped_pc == 0x800861dcu &&
        unknownRa.progress.returned_value.word == 1);

    Fixture unknownS8(1);
    unknownS8.context.registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
        {0xa1b2c3d4u, 5};
    check(unknownS8.run() == NBA97_TEXT_COMPLETE &&
        unknownS8.mask32(Frame + 0x10u) == 5 &&
        unknownS8.progress.restored_s8.known_mask == 5);
}

void RefusalsValidationAndBudgets() {
    Fixture absent(0);
    absent.context.io = nullptr;
    check(absent.run() == NBA97_TEXT_IO_REFUSED &&
        absent.progress.operations == 5 &&
        absent.progress.stopped_pc == 0x800861c4u &&
        absent.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800861ccu);

    Fixture refused(0);
    refused.refuse = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED && refused.events.size() == 1 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            refused.childV0);

    Fixture malformed(0);
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.events.size() == 1 &&
        malformed.progress.operations == 5 &&
        malformed.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A3].known_mask == 16);

    Fixture invalid;
    invalid.context.registers.gpr[0] = {1, 15};
    check(invalid.run() == NBA97_TEXT_ARGUMENT && invalid.progress.operations == 0);
    invalid.context.registers.gpr[0] = {0, 15};
    invalid.context.access_journal = nullptr;
    check(invalid.run() == NBA97_TEXT_ARGUMENT);

    for (std::uint32_t state : {0u, 1u}) {
        const std::size_t total = state ? 6u : 7u;
        for (std::size_t budget = 0; budget <= total + 1u; ++budget) {
            Fixture f(state);
            f.context.operation_budget = budget;
            const int result = f.run();
            if (budget < total) {
                check(result == NBA97_TEXT_LIMIT && !f.progress.completed &&
                    f.progress.operations == budget);
                if (state == 0 && budget == 4)
                    check(f.progress.stopped_pc == 0x800861c4u &&
                        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                            0x800861ccu);
            } else {
                check(result == NBA97_TEXT_COMPLETE && f.progress.completed &&
                    f.progress.operations == total);
            }
        }
    }
}
}

int main() {
    stateAndJournalCoverage();
    fullGprMutationAndAliases();
    pointerMappingAlignmentWrapAndKnownness();
    RefusalsValidationAndBudgets();
    std::printf("%u game audio stream service checks passed\n", checks);
    return 0;
}
