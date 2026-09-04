#include "recovered/game_clock_initialize.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,"game clock-initialize check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x20u;
constexpr std::uint32_t CallerRa=0x80029a54u;
constexpr std::uint32_t IncomingFp=0xf5f5f5f5u;
constexpr std::uint32_t Gp=0x800d79c8u;
constexpr std::uint32_t Guard=0x800c4aa4u;
constexpr std::uint32_t Slots=0x800d6decu;
constexpr std::uint32_t Handler=0x800916b4u;
constexpr std::uint32_t Shutdown=0x8009167cu;
constexpr std::uint32_t Counter=0xf2000002u;
constexpr std::uint32_t ClockBase=0x00409980u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameClockInitializeContext context{{regions,2},100,120,EntrySp,
        CallerRa,IncomingFp,Gp,io,this};
    Nba97GameClockInitializeProgress progress{};
    std::vector<Nba97GameClockInitializeEvent> calls;
    std::size_t refuse_call=std::numeric_limits<std::size_t>::max();
    std::size_t malformed_call=std::numeric_limits<std::size_t>::max();
    std::size_t mutate_rate_call=std::numeric_limits<std::size_t>::max();
    std::uint32_t mutated_rate=60;
    bool mutate_saved_stack=false;
    bool unknown_saved_ra=false;
    bool unknown_rate=false;
    bool critical=false;
    bool interrupt_installed=false;
    bool shutdown_registered=false;
    bool counter_set=false;
    bool counter_started=false;
    std::uint32_t set_target=0;
    std::uint32_t set_mode=0;
    std::uint32_t set_return=1;
    std::uint32_t start_return=1;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(Guard,0);
        put(0x800d7a78u,0x11111111u);
        put(0x800d7a7cu,0x22222222u);
        put(0x800d7a70u,0x33333333u);
        put(Gp+0x164u,0x44444444u);
        put(Gp+0x160u,0x55555555u);
        put(0x800d7a94u,0x66666666u);
        put(0x800d7a98u,0x77777777u);
        for(unsigned i=0;i<8;++i)put(Slots+i*4u,0x88880000u+i);
        for(unsigned i=0;i<32;++i)put(0x800d7234u+i*4u,0);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base && std::uint64_t(address-region.base)<region.size)
                return region.data+(address-region.base);
        check(false);return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base && std::uint64_t(address-region.base)<region.size)
                return region.known ? region.known+(address-region.base):nullptr;
        check(false);return nullptr;
    }
    void put(std::uint32_t address,std::uint32_t value,
        std::uint8_t value_known=1) {
        for(unsigned i=0;i<4;++i) {
            *byte(address+i)=std::uint8_t(value>>(8*i));
            if(auto* mask=known(address+i))*mask=value_known;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(*byte(address+i))<<(8*i);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameClockInitializeEvent* event,
        Nba97GameClockInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(call==f.refuse_call)return 0;
        if(call==f.malformed_call) {value->known=2;return 1;}
        if(call==f.mutate_rate_call)f.put(event->stack_pointer+0x20u,f.mutated_rate);
        if(f.unknown_rate && event->entry==0x800a575cu)
            for(unsigned i=0;i<4;++i)*f.known(event->stack_pointer+0x20u+i)=0;
        value->word=0x90000000u+static_cast<std::uint32_t>(call);
        value->known=1;
        switch(event->entry) {
        case 0x80098394u:
            if(f.critical || event->argument_count)return 0;
            f.critical=true;return 1;
        case 0x8009860cu:
            if(!f.critical || event->argument_count!=2 ||
               event->argument[0]!=6 || event->argument[1]!=Handler)return 0;
            f.interrupt_installed=true;return 1;
        case 0x800a575cu:
            if(!f.critical || event->argument_count!=1 ||
               event->argument[0]!=Shutdown)return 0;
            f.put(0x800d7234u,Shutdown);
            f.shutdown_registered=true;return 1;
        case 0x800983b4u:
            if(!f.critical || event->argument_count!=3 ||
               event->argument[0]!=Counter || event->argument[2]!=0x1000u)return 0;
            f.counter_set=true;f.set_target=event->argument[1];
            f.set_mode=event->argument[2];value->word=f.set_return;return 1;
        case 0x80098488u:
            if(!f.counter_set || event->argument_count!=1 ||
               event->argument[0]!=Counter)return 0;
            f.counter_started=true;value->word=f.start_return;return 1;
        case 0x80098594u:
            if(!f.critical || event->argument_count)return 0;
            f.critical=false;return 1;
        case 0x800a5880u:
            if(f.critical || event->argument_count)return 0;
            f.put(0x800d7a7cu,0);
            f.put(0x800d7a70u,0);
            f.put(event->global_pointer+0x164u,0);
            f.put(event->global_pointer+0x160u,0);
            value->word=0;
            if(f.mutate_saved_stack) {
                f.put(event->stack_pointer+0x1cu,0x2468ace0u);
                f.put(event->stack_pointer+0x18u,0x13579bdfu);
            }
            if(f.unknown_saved_ra)
                for(unsigned i=0;i<4;++i)
                    *f.known(event->stack_pointer+0x1cu+i)=0;
            return 1;
        default:return 0;
        }
    }
    int run() {return nba97_game_clock_initialize(&context,&progress);}
};

void retail_cold_start() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==62 && f.progress.accesses==55 &&
        f.progress.reads==31 && f.progress.stores==24 &&
        f.progress.callbacks_completed==7);
    check(f.progress.incoming_rate==120 && f.progress.live_rate_divisor==120 &&
        f.progress.clock_base==ClockBase && f.progress.timer_target==35280 &&
        f.progress.effective_rate==120);
    check(!f.progress.initialization_guard_before && f.progress.initialized_once &&
        f.progress.callback_slots_cleared==8 &&
        f.progress.interrupt_handler==Handler &&
        f.progress.shutdown_handler==Shutdown &&
        f.progress.root_counter_spec==Counter &&
        f.progress.root_counter_mode==0x1000u);
    check(f.progress.set_rcnt_return==1 && f.progress.set_rcnt_return_known &&
        f.progress.start_rcnt_return==1 && f.progress.start_rcnt_return_known &&
        f.progress.return_v0==0 && f.progress.return_v0_known &&
        !f.progress.trap_code);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp && f.progress.global_pointer==Gp &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.restored_frame_pointer==IncomingFp);
    check(f.get(FrameSp+0x1cu)==CallerRa &&
        f.get(FrameSp+0x18u)==IncomingFp && f.get(FrameSp+0x10u)==8 &&
        f.get(FrameSp+0x20u)==120);
    check(f.get(Guard)==1 && f.get(0x800d7a78u)==0 &&
        f.get(0x800d7a98u)==35280 && f.get(0x800d7a94u)==120);
    for(unsigned i=0;i<8;++i)check(f.get(Slots+i*4u)==0);
    check(f.get(0x800d7234u)==Shutdown && f.get(0x800d7a7cu)==0 &&
        f.get(0x800d7a70u)==0 && f.get(Gp+0x164u)==0 &&
        f.get(Gp+0x160u)==0);
    check(f.interrupt_installed && f.shutdown_registered && f.counter_set &&
        f.counter_started && !f.critical && f.set_target==35280 &&
        f.set_mode==0x1000u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    const std::uint32_t pcs[]={0x800914ecu,0x80091578u,0x80091594u,
        0x8009163cu,0x8009164cu,0x80091654u,0x8009165cu};
    const std::uint32_t entries[]={0x80098394u,0x8009860cu,0x800a575cu,
        0x800983b4u,0x80098488u,0x80098594u,0x800a5880u};
    check(f.calls.size()==7);
    for(unsigned i=0;i<7;++i) {
        const auto& call=f.calls[i];
        check(call.pc==pcs[i] && call.entry==entries[i] &&
            call.return_address==pcs[i]+8u && call.stack_pointer==FrameSp &&
            call.frame_pointer==FrameSp && call.global_pointer==Gp);
    }
    check(f.calls[0].argument_count==0 && f.calls[1].argument_count==2 &&
        f.calls[1].argument[0]==6 && f.calls[1].argument[1]==Handler &&
        f.calls[2].argument_count==1 && f.calls[2].argument[0]==Shutdown);
    check(f.calls[3].argument_count==3 && f.calls[3].argument[0]==Counter &&
        f.calls[3].argument[1]==35280 && f.calls[3].argument[2]==0x1000u &&
        f.calls[4].argument_count==1 && f.calls[4].argument[0]==Counter &&
        f.calls[5].argument_count==0 && f.calls[6].argument_count==0);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.progress.completed);
}

void warm_quantized_and_live_state() {
    Fixture warm;
    warm.put(Guard,1);
    warm.context.requested_rate=100000;
    check(warm.run()==NBA97_TEXT_COMPLETE && warm.progress.completed);
    check(warm.progress.operations==16 && warm.progress.accesses==11 &&
        warm.progress.reads==6 && warm.progress.stores==5 &&
        warm.progress.callbacks_completed==5);
    check(warm.progress.initialization_guard_before &&
        !warm.progress.initialized_once && !warm.progress.callback_slots_cleared &&
        warm.progress.timer_target==42 && warm.progress.effective_rate==100800);
    check(warm.calls.size()==5 && warm.calls[0].entry==0x80098394u &&
        warm.calls[1].entry==0x800983b4u &&
        warm.calls[4].entry==0x800a5880u);
    check(!warm.interrupt_installed && !warm.shutdown_registered &&
        warm.get(Slots)==0x88880000u && warm.get(0x800d7a78u)==0x11111111u);

    Fixture live;
    live.mutate_rate_call=2; /* Exit-handler registration rewrites a0 home. */
    live.mutate_saved_stack=true;
    live.set_return=0xfedcba98u;
    live.start_return=0;
    check(live.run()==NBA97_TEXT_COMPLETE && live.progress.completed);
    check(live.progress.incoming_rate==120 && live.progress.live_rate_divisor==60 &&
        live.progress.timer_target==70560 && live.progress.effective_rate==60 &&
        live.set_target==70560);
    check(live.progress.set_rcnt_return==0xfedcba98u &&
        live.progress.start_rcnt_return==0 &&
        live.progress.restored_return_address==0x2468ace0u &&
        live.progress.restored_frame_pointer==0x13579bdfu);

    Fixture negative;
    negative.put(Guard,1);
    negative.context.requested_rate=UINT32_MAX;
    check(negative.run()==NBA97_TEXT_COMPLETE && negative.progress.completed &&
        negative.progress.timer_target==(uint32_t)-(int32_t)ClockBase &&
        negative.progress.effective_rate==UINT32_MAX);

    Fixture second_negative_one;
    second_negative_one.put(Guard,1);
    second_negative_one.context.requested_rate=
        static_cast<std::uint32_t>(-3000000);
    check(second_negative_one.run()==NBA97_TEXT_COMPLETE &&
        second_negative_one.progress.completed &&
        second_negative_one.progress.timer_target==UINT32_MAX &&
        second_negative_one.progress.effective_rate==
            static_cast<std::uint32_t>(-(int32_t)ClockBase));
}

void source_divide_traps() {
    Fixture zero;
    zero.context.requested_rate=0;
    check(zero.run()==NBA97_GAME_CLOCK_DIVIDE_TRAP && !zero.progress.completed &&
        zero.progress.stopped_pc==0x800915b8u && zero.progress.trap_code==7 &&
        zero.progress.operations==52 && zero.progress.accesses==49 &&
        zero.progress.reads==27 && zero.progress.stores==22 &&
        zero.progress.callbacks_completed==3);
    check(zero.critical && zero.interrupt_installed && zero.shutdown_registered &&
        zero.get(Guard)==1 && zero.get(0x800d7a98u)==0x77777777u);

    Fixture second;
    second.put(Guard,1);
    second.context.requested_rate=ClockBase+1u;
    check(second.run()==NBA97_GAME_CLOCK_DIVIDE_TRAP &&
        second.progress.stopped_pc==0x80091600u &&
        second.progress.trap_code==7 && second.progress.operations==8 &&
        second.progress.accesses==7 && second.progress.reads==3 &&
        second.progress.stores==4 && second.progress.callbacks_completed==1);
    check(second.critical && second.get(0x800d7a98u)==0 &&
        second.get(0x800d7a94u)==0x66666666u && !second.counter_set);

    Fixture most_negative;
    most_negative.put(Guard,1);
    most_negative.context.requested_rate=0x80000000u;
    check(most_negative.run()==NBA97_GAME_CLOCK_DIVIDE_TRAP &&
        most_negative.progress.stopped_pc==0x80091600u &&
        most_negative.get(0x800d7a98u)==0);
}

void limits_and_failures() {
    std::vector<std::uint32_t> pcs={0x800914dcu,0x800914e0u,0x800914e8u,
        0x800914ecu,0x800914f8u,0x8009150cu,0x80091510u};
    for(unsigned i=0;i<8;++i) {
        pcs.push_back(0x80091514u);pcs.push_back(0x80091530u);
        pcs.push_back(0x8009154cu);pcs.push_back(0x80091550u);
        pcs.push_back(0x80091560u);
    }
    pcs.push_back(0x80091514u);
    for(auto pc:{0x80091578u,0x80091588u,0x80091594u,0x800915a4u,
                 0x800915dcu,0x800915ecu,0x80091624u,0x80091634u,
                 0x8009163cu,0x8009164cu,0x80091654u,0x8009165cu,
                 0x80091668u,0x8009166cu})pcs.push_back(pc);
    check(pcs.size()==62);
    for(std::size_t budget=0;budget<pcs.size();++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    for(std::size_t refused=0;refused<7;++refused) {
        Fixture f;f.refuse_call=refused;
        check(f.run()==NBA97_TEXT_IO_REFUSED &&
            f.calls.size()==refused+1 &&
            f.progress.callbacks_completed==refused && !f.progress.completed);
        if(refused>=1)
            for(unsigned i=0;i<8;++i)check(f.get(Slots+i*4u)==0);
    }
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x800914ecu &&
        no_io.progress.stopped_entry==0x80098394u && no_io.progress.stores==3);
    Fixture malformed_return;malformed_return.malformed_call=0;
    check(malformed_return.run()==NBA97_TEXT_ARGUMENT &&
        malformed_return.progress.stopped_pc==0x800914ecu);
    Fixture unknown_guard;
    for(unsigned i=0;i<4;++i)*unknown_guard.known(Guard+i)=0;
    check(unknown_guard.run()==NBA97_TEXT_UNKNOWN &&
        unknown_guard.progress.stopped_pc==0x800914f8u && unknown_guard.critical);
    Fixture unknown_argument;unknown_argument.unknown_rate=true;
    check(unknown_argument.run()==NBA97_TEXT_UNKNOWN &&
        unknown_argument.progress.stopped_pc==0x800915a4u &&
        unknown_argument.progress.callbacks_completed==3);
    Fixture unknown_stack;unknown_stack.unknown_saved_ra=true;
    check(unknown_stack.run()==NBA97_TEXT_UNKNOWN &&
        unknown_stack.progress.stopped_pc==0x80091668u &&
        unknown_stack.progress.stopped_address==FrameSp+0x1cu);
    Fixture malformed_memory;*malformed_memory.known(Slots)=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x8009154cu);
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x800914dcu);
    Fixture missing;missing.context.memory={&missing.regions[1],1};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x800914f8u);
    Fixture overlap;
    Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameClockInitializeProgress progress{};
    check(nba97_game_clock_initialize(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_clock_initialize(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    retail_cold_start();
    warm_quantized_and_live_state();
    source_divide_traps();
    limits_and_failures();
    std::printf("game_clock_initialize: %u checks passed\n",checks);
}
