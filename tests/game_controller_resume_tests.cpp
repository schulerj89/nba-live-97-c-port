#include "recovered/game_controller_resume.h"

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
        std::fprintf(stderr,"game controller-resume check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029a20u;
constexpr std::uint32_t SuspendFlag=0x800c4a70u;
constexpr std::uint32_t ClockSnapshot=0x800c4a74u;
constexpr std::uint32_t PadMode=0x800d7a48u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameControllerResumeContext context{{regions,2},100,8,EntrySp,
        CallerRa,io,this};
    Nba97GameControllerResumeProgress progress{};
    std::vector<Nba97GameControllerResumeEvent> calls;
    std::uint32_t clock=0x13579bdfu;
    std::uint32_t refuse_entry=0;
    std::uint32_t malformed_entry=0;
    bool clock_known=true;
    bool rewrite_ra=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(SuspendFlag,1);put(ClockSnapshot,9);put(PadMode,3);
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
    void put(std::uint32_t address,std::uint32_t value) {
        for(unsigned i=0;i<4;++i) {
            *byte(address+i)=std::uint8_t(value>>(8*i));
            if(auto* mask=known(address+i))*mask=1;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)value|=std::uint32_t(*byte(address+i))<<(8*i);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameControllerResumeEvent* event,
        Nba97GameControllerResumeValue* value) {
        auto& f=*static_cast<Fixture*>(user);f.calls.push_back(*event);
        if(event->entry==f.refuse_entry)return 0;
        if(f.rewrite_ra && event->kind==NBA97_GAME_CONTROLLER_RESUME_CLOCK)
            f.put(event->stack_pointer+0x10u,0x2468ace0u);
        if(event->kind==NBA97_GAME_CONTROLLER_RESUME_INITIALIZE)
            *value={0xdeadbeefu,0};
        else
            *value={f.clock,static_cast<std::uint8_t>(f.clock_known)};
        if(event->entry==f.malformed_entry)value->known=2;
        return 1;
    }
    int run() {return nba97_game_controller_resume(&context,&progress);}
};

void suspended_resume() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.input_reinitialized);
    check(f.progress.operations==8 && f.progress.accesses==6 &&
        f.progress.reads==2 && f.progress.stores==4 &&
        f.progress.callbacks_completed==2);
    check(f.progress.requested_pad_mode==8 && f.progress.initial_suspend_flag==1 &&
        f.progress.clock_snapshot==f.clock && f.progress.clock_snapshot_known &&
        f.progress.return_v0==f.clock && f.progress.return_v0_known);
    check(f.get(SuspendFlag)==0 && f.get(ClockSnapshot)==f.clock &&
        f.get(PadMode)==8 && f.get(FrameSp+0x10u)==CallerRa);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa);
    check(f.calls.size()==2 &&
        f.calls[0].kind==NBA97_GAME_CONTROLLER_RESUME_INITIALIZE &&
        f.calls[0].pc==0x8008f1f4u && f.calls[0].entry==0x80091184u &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].return_address==0x8008f1fcu && !f.calls[0].argument_count);
    check(f.calls[1].kind==NBA97_GAME_CONTROLLER_RESUME_CLOCK &&
        f.calls[1].pc==0x8008f204u && f.calls[1].entry==0x800a5810u &&
        f.calls[1].stack_pointer==FrameSp &&
        f.calls[1].return_address==0x8008f20cu && !f.calls[1].argument_count);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.get(ClockSnapshot)==without_masks.clock);
}

void already_active() {
    Fixture f;f.put(SuspendFlag,0);f.context.pad_mode=2;
    const auto old_clock=f.get(ClockSnapshot);
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        !f.progress.input_reinitialized && f.calls.empty());
    check(f.progress.operations==4 && f.progress.accesses==4 &&
        f.progress.reads==2 && f.progress.stores==2 &&
        !f.progress.callbacks_completed);
    check(f.progress.requested_pad_mode==2 && !f.progress.initial_suspend_flag &&
        f.get(PadMode)==2 && f.get(SuspendFlag)==0 &&
        f.get(ClockSnapshot)==old_clock);
    check(f.progress.return_v0==0 && f.progress.return_v0_known &&
        !f.progress.clock_snapshot_known &&
        f.progress.restored_return_address==CallerRa);
}

void live_source_order() {
    Fixture gate_alias;
    gate_alias.context.stack_pointer=SuspendFlag+8u;
    check(gate_alias.run()==NBA97_TEXT_COMPLETE &&
        gate_alias.progress.input_reinitialized && gate_alias.calls.size()==2);
    check(gate_alias.progress.frame_stack_pointer==SuspendFlag-0x10u &&
        gate_alias.progress.initial_suspend_flag==1 &&
        gate_alias.progress.restored_return_address==0 &&
        gate_alias.get(SuspendFlag)==0);

    Fixture mode_alias;mode_alias.put(SuspendFlag,0);
    mode_alias.context.stack_pointer=PadMode+8u;
    mode_alias.context.pad_mode=0x10203040u;
    check(mode_alias.run()==NBA97_TEXT_COMPLETE &&
        mode_alias.progress.restored_return_address==0x10203040u &&
        mode_alias.get(PadMode)==0x10203040u);

    Fixture rewrite;rewrite.rewrite_ra=true;
    check(rewrite.run()==NBA97_TEXT_COMPLETE &&
        rewrite.progress.restored_return_address==0x2468ace0u &&
        rewrite.get(FrameSp+0x10u)==0x2468ace0u);
}

void unknown_clock_and_refusals() {
    Fixture unknown;unknown.clock=0x89abcdefu;unknown.clock_known=false;
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.progress.completed &&
        unknown.progress.input_reinitialized && !unknown.progress.clock_snapshot_known &&
        unknown.progress.return_v0==0x89abcdefu && !unknown.progress.return_v0_known &&
        unknown.get(ClockSnapshot)==0x89abcdefu);
    for(unsigned i=0;i<4;++i)check(*unknown.known(ClockSnapshot+i)==0);

    Fixture unrepresentable;unrepresentable.clock_known=false;
    unrepresentable.regions[0].known=nullptr;
    unrepresentable.regions[1].known=nullptr;
    check(unrepresentable.run()==NBA97_TEXT_ARGUMENT &&
        unrepresentable.progress.stopped_pc==0x8008f210u &&
        unrepresentable.progress.operations==7 &&
        unrepresentable.progress.callbacks_completed==2 &&
        unrepresentable.progress.stores==3);

    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x8008f1f4u &&
        no_io.progress.stopped_entry==0x80091184u);
    Fixture refuse_init;refuse_init.refuse_entry=0x80091184u;
    check(refuse_init.run()==NBA97_TEXT_IO_REFUSED &&
        refuse_init.calls.size()==1 && !refuse_init.progress.callbacks_completed);
    Fixture refuse_clock;refuse_clock.refuse_entry=0x800a5810u;
    check(refuse_clock.run()==NBA97_TEXT_IO_REFUSED &&
        refuse_clock.calls.size()==2 && refuse_clock.progress.callbacks_completed==1 &&
        refuse_clock.get(SuspendFlag)==0);
    Fixture malformed_init;malformed_init.malformed_entry=0x80091184u;
    check(malformed_init.run()==NBA97_TEXT_ARGUMENT &&
        !malformed_init.progress.callbacks_completed);
    Fixture malformed_clock;malformed_clock.malformed_entry=0x800a5810u;
    check(malformed_clock.run()==NBA97_TEXT_ARGUMENT &&
        malformed_clock.progress.callbacks_completed==1);
}

void limits_and_memory() {
    constexpr std::uint32_t pcs[]={0x8008f1d8u,0x8008f1e0u,0x8008f1e8u,
        0x8008f1f4u,0x8008f200u,0x8008f204u,0x8008f210u,0x8008f214u};
    for(std::size_t budget=0;budget<8;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    Fixture short_active;short_active.put(SuspendFlag,0);
    short_active.context.operation_budget=3;
    check(short_active.run()==NBA97_TEXT_LIMIT &&
        short_active.progress.stopped_pc==0x8008f214u);
    Fixture unknown_flag;*unknown_flag.known(SuspendFlag)=0;
    check(unknown_flag.run()==NBA97_TEXT_UNKNOWN &&
        unknown_flag.progress.stopped_pc==0x8008f1d8u &&
        !unknown_flag.progress.reads);
    Fixture malformed_flag;*malformed_flag.known(SuspendFlag)=2;
    check(malformed_flag.run()==NBA97_TEXT_ARGUMENT &&
        malformed_flag.progress.stopped_pc==0x8008f1d8u);
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x8008f1e0u);
    Fixture missing;missing.context.stack_pointer=0x90000020u;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x8008f1e0u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameControllerResumeProgress progress{};
    check(nba97_game_controller_resume(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_controller_resume(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    suspended_resume();
    already_active();
    live_source_order();
    unknown_clock_and_refusals();
    limits_and_memory();
    std::printf("game_controller_resume: %u checks passed\n",checks);
}
