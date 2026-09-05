#include "recovered/game_frame_rate_reset.h"

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
        std::fprintf(stderr,"game frame-rate reset check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Gp=0x800d79c8u;
constexpr std::uint32_t Globals=0x800d7b30u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807ffff8u;
constexpr std::uint32_t FrameCounter=0x800d7b44u;
constexpr std::uint32_t Auxiliary=0x800d7b48u;
constexpr std::uint32_t Baseline=0x800d7b4cu;
constexpr std::uint32_t Instantaneous=0x800d7b50u;
constexpr std::uint32_t Average=0x800d7b54u;
constexpr std::uint32_t LastReport=0x800d7b58u;

struct Fixture {
    std::array<std::uint8_t,0x40> globals{},globals_known{};
    std::array<std::uint8_t,0x100> stack{},stack_known{};
    Nba97GameTextRegion regions[2]={
        {Globals,globals.data(),globals_known.data(),globals.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameFrameRateResetContext context{{regions,2},20,EntrySp,
        0x11223344u,Gp,io,this};
    Nba97GameFrameRateResetProgress progress{};
    std::vector<Nba97GameFrameRateResetEvent> events;
    std::uint32_t clock=0x13579bdfu;
    std::uint8_t clock_known=1;
    bool refuse=false;
    bool mutate=false;
    bool observed_source_order=false;

    Fixture() {
        globals.fill(0xcd);globals_known.fill(1);
        stack.fill(0xcd);stack_known.fill(1);
        put(FrameCounter,9);put(Auxiliary,0x11111111u);
        put(Baseline,0x22222222u);put(Instantaneous,0x33333333u);
        put(Average,0x44444444u);put(LastReport,0x55555555u);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return region.data+(address-region.base);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return region.known ? region.known+(address-region.base):nullptr;
        return nullptr;
    }
    void put(std::uint32_t address,std::uint32_t value) {
        for(unsigned i=0;i<4;++i) {
            *byte(address+i)=static_cast<std::uint8_t>(value>>(i*8u));
            if(known(address+i))*known(address+i)=1;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(*byte(address+i))<<(i*8u);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameFrameRateResetEvent* event,
        Nba97GameFrameRateResetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.events.push_back(*event);
        if(f.refuse)return 0;
        f.observed_source_order=f.get(FrameCounter)==0 &&
            f.get(Auxiliary)==0 && f.get(Baseline)==0x22222222u &&
            f.get(Instantaneous)==0 && f.get(Average)==0 &&
            f.get(LastReport)==0;
        if(f.mutate) {
            f.put(FrameCounter,0xabcdef01u);
            f.put(EntrySp-8u,0x55667788u);
        }
        *value={f.clock,f.clock_known};
        return 1;
    }
    int run() {return nba97_game_frame_rate_reset(&context,&progress);}
};

void exact_reset_and_clock_seed() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.get(FrameCounter)==0 && f.get(Auxiliary)==0 &&
        f.get(Baseline)==f.clock && f.get(Instantaneous)==0 &&
        f.get(Average)==0 && f.get(LastReport)==0);
    check(f.observed_source_order && f.events.size()==1);
    check(f.events[0].kind==NBA97_GAME_FRAME_RATE_RESET_READ_CLOCK &&
        f.events[0].pc==0x800a7754u && f.events[0].entry==0x800a5810u &&
        f.events[0].stack_pointer==EntrySp-0x18u &&
        f.events[0].global_pointer==Gp &&
        f.events[0].return_address==0x800a775cu &&
        f.events[0].argument_count==0);
    check(f.progress.operations==9 && f.progress.accesses==8 &&
        f.progress.reads==1 && f.progress.stores==7 &&
        f.progress.callbacks_completed==1);
    check(f.progress.frame_stack_pointer==EntrySp-0x18u &&
        f.progress.stack_pointer==EntrySp && f.progress.global_pointer==Gp &&
        f.progress.restored_return_address==0x11223344u);
    check(f.progress.frame_counter_address==FrameCounter &&
        f.progress.auxiliary_address==Auxiliary &&
        f.progress.clock_baseline_address==Baseline &&
        f.progress.instantaneous_rate_address==Instantaneous &&
        f.progress.average_rate_address==Average &&
        f.progress.last_report_clock_address==LastReport);
    check(f.progress.sampled_clock==f.clock &&
        f.progress.sampled_clock_known && f.progress.return_v0==f.clock &&
        f.progress.return_v0_known && !f.progress.stopped_pc &&
        !f.progress.stopped_address && !f.progress.stopped_entry);
    check(f.get(EntrySp-8u)==0x11223344u);
    check(*f.byte(FrameCounter-1u)==0xcd && *f.byte(LastReport+4u)==0xcd);
}

void source_order_and_live_epilogue() {
    Fixture f;f.mutate=true;f.clock=0x01020304u;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.observed_source_order && f.get(FrameCounter)==0xabcdef01u &&
        f.get(Baseline)==0x01020304u);
    check(f.progress.restored_return_address==0x55667788u &&
        f.progress.return_v0==0x01020304u);

    /* Deliberately alias saved-ra with the first GP-relative word. The source
       saves ra first, then its 0x800A7740 clear wins, so the epilogue loads 0. */
    Fixture alias;
    alias.context.memory={alias.regions,1};
    alias.context.stack_pointer=FrameCounter+8u;
    alias.context.io=Fixture::io;
    check(alias.run()==NBA97_TEXT_COMPLETE && alias.progress.completed);
    check(alias.progress.frame_stack_pointer==FrameCounter-0x10u &&
        alias.progress.restored_return_address==0 &&
        alias.progress.stack_pointer==FrameCounter+8u);
}

void unknownness_and_callback_failures() {
    Fixture unknown;unknown.clock_known=0;
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.progress.completed &&
        !unknown.progress.sampled_clock_known &&
        !unknown.progress.return_v0_known && unknown.get(Baseline)==unknown.clock);
    check(*unknown.known(Baseline)==0 && *unknown.known(Baseline+3u)==0 &&
        *unknown.known(FrameCounter)==1 && *unknown.known(LastReport+3u)==1);

    Fixture no_masks;no_masks.regions[0].known=nullptr;
    no_masks.regions[1].known=nullptr;
    check(no_masks.run()==NBA97_TEXT_COMPLETE && no_masks.get(Baseline)==no_masks.clock);

    Fixture unrepresentable;unrepresentable.regions[0].known=nullptr;
    unrepresentable.clock_known=0;
    check(unrepresentable.run()==NBA97_TEXT_ARGUMENT &&
        !unrepresentable.progress.completed &&
        unrepresentable.progress.callbacks_completed==1 &&
        unrepresentable.progress.stopped_pc==0x800a775cu &&
        unrepresentable.progress.stopped_address==Baseline);

    Fixture missing;missing.context.io=nullptr;
    check(missing.run()==NBA97_TEXT_IO_REFUSED &&
        missing.progress.operations==7 && missing.progress.stores==6 &&
        missing.progress.stopped_pc==0x800a7754u &&
        missing.progress.stopped_entry==0x800a5810u);
    Fixture refused;refused.refuse=true;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.events.size()==1 &&
        !refused.progress.callbacks_completed);
    Fixture malformed;malformed.clock_known=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.events.size()==1 &&
        !malformed.progress.callbacks_completed &&
        malformed.progress.stopped_entry==0x800a5810u);
}

void bounded_prefixes_and_memory_failures() {
    static constexpr std::uint32_t stopped_pc[9]={0x800a773cu,
        0x800a7740u,0x800a7744u,0x800a7748u,0x800a774cu,0x800a7750u,
        0x800a7754u,0x800a775cu,0x800a7760u};
    for(std::size_t budget=0;budget<9;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.stopped_pc==stopped_pc[budget]);
        check(f.progress.stores==(budget<6 ? budget:budget==6 ? 6:budget-1) &&
            f.progress.callbacks_completed==(budget>=7 ? 1u:0u));
    }

    Fixture missing_stack;missing_stack.context.memory={missing_stack.regions,1};
    check(missing_stack.run()==NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc==0x800a773cu &&
        missing_stack.progress.stopped_address==EntrySp-8u);
    Fixture missing_globals;missing_globals.context.memory.region++;
    missing_globals.context.memory.count=1;
    check(missing_globals.run()==NBA97_TEXT_RESOURCE &&
        missing_globals.progress.stores==1 &&
        missing_globals.progress.stopped_pc==0x800a7740u &&
        missing_globals.progress.stopped_address==FrameCounter);
    Fixture bad_known;*bad_known.known(Average+2u)=2;
    check(bad_known.run()==NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stores==4 &&
        bad_known.progress.stopped_pc==0x800a774cu);
    Fixture unaligned;unaligned.context.global_pointer++;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stores==1 &&
        unaligned.progress.stopped_pc==0x800a7740u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],
        overlap.regions[0]};overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.regions[0].size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT && !null_regions.progress.operations);
    Fixture wraps;wraps.regions[0].base=0xfffffffcu;wraps.regions[0].size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameFrameRateResetProgress progress{};
    check(nba97_game_frame_rate_reset(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_frame_rate_reset(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_reset_and_clock_seed();
    source_order_and_live_epilogue();
    unknownness_and_callback_failures();
    bounded_prefixes_and_memory_failures();
    std::printf("game_frame_rate_reset: %u checks passed\n",checks);
}
