#include "recovered/game_reset_callback.h"

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
        std::fprintf(stderr,"game reset-callback check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029a18u;
constexpr std::uint32_t DispatchPointer=0x800c54c8u;
constexpr std::uint32_t DispatchTable=0x800c54b0u;
constexpr std::uint32_t DispatchTarget=0x80098714u;

struct Fixture {
    enum Mode {Return,Refuse,Malformed,RewriteRa} mode=Return;
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameResetCallbackContext context{{regions,2},100,EntrySp,CallerRa,io,this};
    Nba97GameResetCallbackProgress progress{};
    std::vector<Nba97GameResetCallbackEvent> calls;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(DispatchPointer,DispatchTable);put(DispatchTable+0x0cu,DispatchTarget);
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
            auto* mask=known(address+i);
            *byte(address+i)=std::uint8_t(value>>(8*i));
            if(mask)*mask=1;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t result=0;
        for(unsigned i=0;i<4;++i)result|=std::uint32_t(*byte(address+i))<<(8*i);
        return result;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameResetCallbackEvent* event,Nba97GameResetCallbackValue* value) {
        auto& f=*static_cast<Fixture*>(user);f.calls.push_back(*event);
        if(f.mode==Refuse)return 0;
        if(f.mode==RewriteRa)f.put(event->stack_pointer+0x10u,0x12345678u);
        value->word=0x2468ace0u;
        value->known=f.mode==Malformed ? 2 : (f.mode==RewriteRa ? 0 : 1);
        return 1;
    }
    int run() {return nba97_game_reset_callback(&context,&progress);}
};

void retail_dispatch() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.calls.size()==1 && f.calls[0].pc==0x800985f4u &&
        f.calls[0].entry==DispatchTarget && f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].return_address==0x800985fcu && !f.calls[0].argument_count);
    check(f.progress.operations==5 && f.progress.accesses==4 &&
        f.progress.reads==3 && f.progress.stores==1 &&
        f.progress.callbacks_completed==1);
    check(f.progress.dispatch_table==DispatchTable &&
        f.progress.dispatch_target==DispatchTarget);
    check(f.get(FrameSp+0x10u)==CallerRa &&
        f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa);
    check(f.progress.return_v0==0x2468ace0u && f.progress.return_v0_known);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.progress.dispatch_target==DispatchTarget);
}

void live_source_order() {
    Fixture alias;
    alias.context.stack_pointer=DispatchTable+0x14u;
    alias.context.return_address=0x89abcdefu;
    check(alias.run()==NBA97_TEXT_COMPLETE && alias.calls.size()==1);
    check(alias.progress.frame_stack_pointer==DispatchTable-4u &&
        alias.calls[0].entry==0x89abcdefu &&
        alias.progress.dispatch_target==0x89abcdefu);
    check(alias.get(DispatchTable+0x0cu)==0x89abcdefu &&
        alias.progress.restored_return_address==0x89abcdefu);

    Fixture rewrite;rewrite.mode=Fixture::RewriteRa;
    check(rewrite.run()==NBA97_TEXT_COMPLETE && rewrite.progress.completed);
    check(rewrite.progress.restored_return_address==0x12345678u &&
        rewrite.get(FrameSp+0x10u)==0x12345678u);
    check(rewrite.progress.return_v0==0x2468ace0u &&
        !rewrite.progress.return_v0_known);
}

void limits_and_failures() {
    {Fixture f;f.context.operation_budget=0;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985e0u &&
        f.progress.stopped_address==DispatchPointer && !f.progress.operations);}
    {Fixture f;f.context.operation_budget=1;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985e8u &&
        f.progress.stopped_address==FrameSp+0x10u && f.progress.reads==1);}
    {Fixture f;f.context.operation_budget=2;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985ecu && f.progress.stores==1);}
    {Fixture f;f.context.operation_budget=3;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985f4u &&
        f.progress.stopped_entry==DispatchTarget && f.calls.empty());}
    {Fixture f;f.context.operation_budget=4;check(f.run()==NBA97_TEXT_LIMIT &&
        f.progress.stopped_pc==0x800985fcu && f.calls.size()==1 &&
        f.progress.callbacks_completed==1 && !f.progress.completed);}
    {Fixture f;f.context.io=nullptr;check(f.run()==NBA97_TEXT_IO_REFUSED &&
        f.progress.stopped_pc==0x800985f4u && !f.progress.callbacks_completed);}
    {Fixture f;f.mode=Fixture::Refuse;check(f.run()==NBA97_TEXT_IO_REFUSED &&
        f.calls.size()==1 && !f.progress.callbacks_completed);}
    {Fixture f;f.mode=Fixture::Malformed;check(f.run()==NBA97_TEXT_ARGUMENT &&
        f.calls.size()==1 && !f.progress.callbacks_completed);}
    {Fixture f;*f.known(DispatchPointer)=0;check(f.run()==NBA97_TEXT_UNKNOWN &&
        f.progress.stopped_pc==0x800985e0u && !f.progress.reads);}
    {Fixture f;*f.known(DispatchTable+0x0cu)=0;check(f.run()==NBA97_TEXT_UNKNOWN &&
        f.progress.stopped_pc==0x800985ecu && f.progress.reads==1 && f.progress.stores==1);}
    {Fixture f;f.put(DispatchPointer,DispatchTable+1u);check(
        f.run()==NBA97_TEXT_ALIGNMENT_TRAP && f.progress.stopped_pc==0x800985ecu);}
    {Fixture f;f.context.stack_pointer=0x90000020u;check(f.run()==NBA97_TEXT_RESOURCE &&
        f.progress.stopped_pc==0x800985e8u && f.progress.reads==1);}
    {Fixture f;*f.known(DispatchPointer)=2;check(f.run()==NBA97_TEXT_ARGUMENT &&
        f.progress.stopped_pc==0x800985e0u);}
    {Fixture f;Nba97GameTextRegion overlap[2]={f.regions[0],f.regions[0]};
        f.context.memory={overlap,2};check(f.run()==NBA97_TEXT_ARGUMENT &&
        !f.progress.operations);}
    {Fixture f;f.regions[0].size=0;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.regions[0].data=nullptr;check(f.run()==NBA97_TEXT_ARGUMENT);}
    {Fixture f;f.context.memory={nullptr,1};check(f.run()==NBA97_TEXT_ARGUMENT);}
    Nba97GameResetCallbackProgress progress{};
    check(nba97_game_reset_callback(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;check(nba97_game_reset_callback(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    retail_dispatch();
    live_source_order();
    limits_and_failures();
    std::printf("game_reset_callback: %u checks passed\n",checks);
}
