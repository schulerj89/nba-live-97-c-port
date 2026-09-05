#include "recovered/game_controller_suspend.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value,unsigned line) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,"game controller-suspend check %u failed at line %u\n",
            checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029b7cu;
constexpr std::uint32_t SuspendFlag=0x800c4a70u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameControllerSuspendContext context{{regions,2},20,EntrySp,
        CallerRa,io,this};
    Nba97GameControllerSuspendProgress progress{};
    std::vector<Nba97GameControllerSuspendEvent> calls;
    std::uint32_t child_value=0xdeadbeefu;
    std::uint32_t rewritten_ra=0;
    std::uint32_t rewritten_flag=0;
    std::uint32_t refuse_entry=0;
    bool child_known=true;
    bool malformed=false;
    bool rewrite_ra=false;
    bool rewrite_flag=false;
    bool forget_ra=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);put(SuspendFlag,0);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return region.data+(address-region.base);
        check(false);return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return region.known?region.known+(address-region.base):nullptr;
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
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(*byte(address+i))<<(8*i);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameControllerSuspendEvent* event,
        Nba97GameControllerSuspendValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if(event->entry==f.refuse_entry)return 0;
        if(f.rewrite_ra)f.put(event->stack_pointer+0x10u,f.rewritten_ra);
        if(f.rewrite_flag)f.put(SuspendFlag,f.rewritten_flag);
        if(f.forget_ra)
            for(unsigned i=0;i<4;++i)
                *f.known(event->stack_pointer+0x10u+i)=0;
        *value={f.child_value,static_cast<std::uint8_t>(f.child_known)};
        if(f.malformed)value->known=2;
        return 1;
    }
    int run() {return nba97_game_controller_suspend(&context,&progress);}
};

void active_shutdown() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.shutdown_called && f.progress.input_suspended);
    check(f.progress.operations==5 && f.progress.accesses==4 &&
        f.progress.reads==2 && f.progress.stores==2 &&
        f.progress.callbacks_completed==1);
    check(f.progress.initial_suspend_flag==0 && f.progress.return_v0==1 &&
        f.progress.return_v0_known && f.get(SuspendFlag)==1);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa &&
        f.get(FrameSp+0x10u)==CallerRa);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_CONTROLLER_SUSPEND_SHUTDOWN &&
        f.calls[0].pc==0x8008f1b0u && f.calls[0].entry==0x80091224u &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].return_address==0x8008f1b8u &&
        !f.calls[0].argument_count);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.get(SuspendFlag)==1);
}

void already_suspended() {
    for(std::uint32_t flag:{1u,0xdeadbeefu}) {
        Fixture f;f.put(SuspendFlag,flag);
        check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
            !f.progress.shutdown_called && f.progress.input_suspended &&
            f.calls.empty());
        check(f.progress.operations==3 && f.progress.accesses==3 &&
            f.progress.reads==2 && f.progress.stores==1 &&
            !f.progress.callbacks_completed);
        check(f.progress.initial_suspend_flag==flag &&
            f.progress.return_v0==flag && f.progress.return_v0_known &&
            f.get(SuspendFlag)==flag &&
            f.progress.restored_return_address==CallerRa);
    }
}

void child_and_live_memory_quirks() {
    Fixture arbitrary;arbitrary.child_value=0x76543210u;
    check(arbitrary.run()==NBA97_TEXT_COMPLETE &&
        arbitrary.progress.return_v0==1 && arbitrary.progress.return_v0_known);
    Fixture unknown;unknown.child_value=0x89abcdefu;unknown.child_known=false;
    check(unknown.run()==NBA97_TEXT_COMPLETE &&
        unknown.progress.return_v0==1 && unknown.progress.return_v0_known);

    Fixture rewrite;rewrite.rewrite_ra=true;rewrite.rewritten_ra=0x2468ace0u;
    check(rewrite.run()==NBA97_TEXT_COMPLETE &&
        rewrite.progress.restored_return_address==0x2468ace0u &&
        rewrite.get(FrameSp+0x10u)==0x2468ace0u);
    Fixture flag_rewrite;flag_rewrite.rewrite_flag=true;
    flag_rewrite.rewritten_flag=0xabcdef01u;
    check(flag_rewrite.run()==NBA97_TEXT_COMPLETE &&
        flag_rewrite.get(SuspendFlag)==1 &&
        flag_rewrite.progress.return_v0==1);

    Fixture unknown_ra;unknown_ra.forget_ra=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc==0x8008f1c4u &&
        unknown_ra.progress.operations==5 &&
        unknown_ra.progress.callbacks_completed==1 &&
        unknown_ra.progress.shutdown_called && unknown_ra.progress.input_suspended &&
        !unknown_ra.progress.completed);

    /* The source flag load predates frame allocation. Here saved ra aliases
       that flag: the active branch still runs, then the final flag store makes
       the live epilogue return address exactly one. */
    Fixture alias;alias.context.stack_pointer=SuspendFlag+8u;
    check(alias.run()==NBA97_TEXT_COMPLETE && alias.calls.size()==1 &&
        alias.progress.initial_suspend_flag==0 && alias.progress.shutdown_called &&
        alias.progress.frame_stack_pointer==SuspendFlag-0x10u &&
        alias.progress.restored_return_address==1 &&
        alias.get(SuspendFlag)==1);
}

void refusals_unknowns_and_limits() {
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x8008f1b0u &&
        no_io.progress.stopped_entry==0x80091224u &&
        no_io.progress.operations==3 && !no_io.progress.callbacks_completed);
    Fixture refuse;refuse.refuse_entry=0x80091224u;
    check(refuse.run()==NBA97_TEXT_IO_REFUSED && refuse.calls.size()==1 &&
        !refuse.progress.callbacks_completed && !refuse.progress.shutdown_called);
    Fixture malformed;malformed.malformed=true;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed && !malformed.progress.shutdown_called);

    Fixture unknown_flag;*unknown_flag.known(SuspendFlag)=0;
    check(unknown_flag.run()==NBA97_TEXT_UNKNOWN &&
        unknown_flag.progress.stopped_pc==0x8008f1a0u &&
        unknown_flag.progress.stopped_address==SuspendFlag &&
        unknown_flag.progress.operations==1 && unknown_flag.progress.accesses==1 &&
        !unknown_flag.progress.reads &&
        unknown_flag.progress.stack_pointer==EntrySp);
    Fixture malformed_flag;*malformed_flag.known(SuspendFlag)=2;
    check(malformed_flag.run()==NBA97_TEXT_ARGUMENT &&
        malformed_flag.progress.stopped_pc==0x8008f1a0u);

    constexpr std::uint32_t active_pcs[]={0x8008f1a0u,0x8008f1acu,
        0x8008f1b0u,0x8008f1c0u,0x8008f1c4u};
    for(std::size_t budget=0;budget<5;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==active_pcs[budget] && !f.progress.completed);
    }
    Fixture active;active.context.operation_budget=5;
    check(active.run()==NBA97_TEXT_COMPLETE && active.progress.operations==5);

    constexpr std::uint32_t warm_pcs[]={0x8008f1a0u,0x8008f1acu,0x8008f1c4u};
    for(std::size_t budget=0;budget<3;++budget) {
        Fixture f;f.put(SuspendFlag,1);f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==warm_pcs[budget] && !f.progress.completed);
    }
    Fixture warm;warm.put(SuspendFlag,1);warm.context.operation_budget=3;
    check(warm.run()==NBA97_TEXT_COMPLETE && warm.progress.operations==3);
}

void invalid_memory_and_arguments() {
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x8008f1acu &&
        unaligned.progress.operations==2);
    Fixture missing;missing.context.stack_pointer=0x90000020u;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x8008f1acu);
    Fixture overlap;
    Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture wrapping;wrapping.regions[0].base=0xfffffffcu;
    wrapping.regions[0].size=8;
    check(wrapping.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameControllerSuspendProgress progress{};
    check(nba97_game_controller_suspend(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_controller_suspend(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    active_shutdown();
    already_suspended();
    child_and_live_memory_quirks();
    refusals_unknowns_and_limits();
    invalid_memory_and_arguments();
    std::printf("game_controller_suspend: %u checks passed\n",checks);
}
