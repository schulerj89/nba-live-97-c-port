#include "recovered/game_graph_debug_set.h"

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
        std::fprintf(stderr,"game graph-debug set check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029a30u;
constexpr std::uint32_t Format=0x80028250u;
constexpr std::uint32_t CallbackPointer=0x800c55bcu;
constexpr std::uint32_t Type=0x800c55c0u;
constexpr std::uint32_t Level=0x800c55c2u;
constexpr std::uint32_t Reverse=0x800c55c3u;
constexpr std::uint32_t Diagnostic=0x8009cb2cu;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameGraphDebugSetContext context{{regions,2},100,0,EntrySp,CallerRa,
        0xa0a0a0a0u,io,this};
    Nba97GameGraphDebugSetProgress progress{};
    std::vector<Nba97GameGraphDebugSetEvent> calls;
    std::size_t refuse_call=std::numeric_limits<std::size_t>::max();
    bool rewrite_stack=false;
    bool rewrite_level=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(CallbackPointer,Diagnostic);
        put(Type,0,1);
        put(Level,0,1);
        put(Reverse,0,1);
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
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4,
        std::uint8_t value_known=1) {
        for(unsigned i=0;i<width;++i) {
            *byte(address+i)=std::uint8_t(value>>(8*i));
            if(auto* mask=known(address+i))*mask=value_known;
        }
    }
    std::uint32_t get(std::uint32_t address,unsigned width=4) {
        std::uint32_t value=0;
        for(unsigned i=0;i<width;++i)
            value|=std::uint32_t(*byte(address+i))<<(8*i);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameGraphDebugSetEvent* event) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(call==f.refuse_call)return 0;
        if(f.rewrite_stack) {
            f.put(event->stack_pointer+0x14u,0x2468ace0u);
            f.put(event->stack_pointer+0x10u,0x13579bdfu);
        }
        if(f.rewrite_level)f.put(Level,0x7fu,1);
        return 1;
    }
    int run() {return nba97_game_graph_debug_set(&context,&progress);}
};

void retail_level_zero() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        !f.progress.diagnostic_called);
    check(f.progress.operations==6 && f.progress.accesses==6 &&
        f.progress.reads==3 && f.progress.stores==3 &&
        !f.progress.callbacks_completed);
    check(f.progress.requested_level==0 && f.progress.previous_level==0 &&
        f.progress.previous_level_known && f.progress.published_level==0 &&
        f.progress.return_v0==0 && f.progress.return_v0_known);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.restored_saved_register_s0==0xa0a0a0a0u);
    check(f.get(FrameSp+0x14u)==CallerRa &&
        f.get(FrameSp+0x10u)==0xa0a0a0a0u && f.get(Level,1)==0 &&
        f.calls.empty());
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.progress.return_v0_known &&
        without_masks.get(Level,1)==0);
}

void diagnostic_path_and_live_epilogue() {
    Fixture f;
    f.put(Level,7,1);f.put(Type,2,1);f.put(Reverse,1,1);
    f.context.level=0x12345603u;f.rewrite_stack=true;f.rewrite_level=true;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.diagnostic_called);
    check(f.progress.operations==11 && f.progress.accesses==10 &&
        f.progress.reads==7 && f.progress.stores==3 &&
        f.progress.callbacks_completed==1);
    check(f.progress.requested_level==0x12345603u &&
        f.progress.previous_level==7 && f.progress.previous_level_known &&
        f.progress.published_level==3 && f.progress.graph_type==2 &&
        f.progress.graph_reverse==1 &&
        f.progress.diagnostic_callback==Diagnostic);
    check(f.progress.return_v0==7 && f.progress.return_v0_known &&
        f.progress.restored_return_address==0x2468ace0u &&
        f.progress.restored_saved_register_s0==0x13579bdfu &&
        f.get(Level,1)==0x7fu);
    check(f.calls.size()==1 && f.calls[0].pc==0x80099310u &&
        f.calls[0].entry==Diagnostic && f.calls[0].argument_count==4 &&
        f.calls[0].argument[0]==Format && f.calls[0].argument[1]==3 &&
        f.calls[0].argument[2]==2 && f.calls[0].argument[3]==1);
    check(f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].return_address==0x80099318u &&
        f.calls[0].saved_register_s0==7 &&
        f.calls[0].saved_register_s0_known);
}

void source_quirks_and_unknown_return() {
    /* The original sb/andi pair makes 0x100 indistinguishable from zero. */
    Fixture alias_zero;alias_zero.put(Level,5,1);alias_zero.context.level=0x100u;
    check(alias_zero.run()==NBA97_TEXT_COMPLETE &&
        alias_zero.progress.published_level==0 &&
        !alias_zero.progress.diagnostic_called && alias_zero.calls.empty() &&
        alias_zero.progress.return_v0==5 && alias_zero.get(Level,1)==0);

    Fixture alias_nonzero;alias_nonzero.context.level=0xffffff01u;
    check(alias_nonzero.run()==NBA97_TEXT_COMPLETE &&
        alias_nonzero.progress.published_level==1 &&
        alias_nonzero.calls.size()==1 &&
        alias_nonzero.calls[0].argument[1]==1);

    /* Preserve the source's unguarded jalr: zero is still offered to the host
       boundary instead of being repaired or replaced with the retail target. */
    Fixture zero_target;zero_target.context.level=1;
    zero_target.put(CallbackPointer,0);
    check(zero_target.run()==NBA97_TEXT_COMPLETE &&
        zero_target.calls.size()==1 && zero_target.calls[0].entry==0 &&
        zero_target.progress.diagnostic_callback==0);

    Fixture unknown_previous;unknown_previous.context.level=0;
    *unknown_previous.known(Level)=0;
    check(unknown_previous.run()==NBA97_TEXT_COMPLETE &&
        unknown_previous.progress.previous_level==0 &&
        !unknown_previous.progress.previous_level_known &&
        unknown_previous.progress.return_v0==0 &&
        !unknown_previous.progress.return_v0_known &&
        *unknown_previous.known(Level)==1 && unknown_previous.get(Level,1)==0);
}

void failures_and_limits() {
    const std::uint32_t pcs[]={0x800992d0u,0x800992d4u,0x800992d8u,
        0x800992dcu,0x800992f0u,0x800992f4u,0x800992fcu,
        0x80099304u,0x80099310u,0x8009931cu,0x80099320u};
    for(std::size_t budget=0;budget<11;++budget) {
        Fixture f;f.context.level=1;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    Fixture no_io;no_io.context.level=1;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x80099310u &&
        no_io.progress.stopped_entry==Diagnostic &&
        !no_io.progress.callbacks_completed);
    Fixture refused;refused.context.level=1;refused.refuse_call=0;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed);
    Fixture unaligned_target;unaligned_target.context.level=1;
    unaligned_target.put(CallbackPointer,Diagnostic+1u);
    check(unaligned_target.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_target.progress.stopped_pc==0x80099310u &&
        unaligned_target.progress.stopped_entry==Diagnostic+1u &&
        unaligned_target.calls.empty());
    Fixture unknown_callback;unknown_callback.context.level=1;
    *unknown_callback.known(CallbackPointer)=0;
    check(unknown_callback.run()==NBA97_TEXT_UNKNOWN &&
        unknown_callback.progress.stopped_pc==0x800992f0u &&
        unknown_callback.progress.stopped_address==CallbackPointer);
    Fixture unknown_type;unknown_type.context.level=1;
    *unknown_type.known(Type)=0;
    check(unknown_type.run()==NBA97_TEXT_UNKNOWN &&
        unknown_type.progress.stopped_pc==0x800992fcu &&
        unknown_type.get(Level,1)==1 && unknown_type.calls.empty());
    Fixture unknown_reverse;unknown_reverse.context.level=1;
    *unknown_reverse.known(Reverse)=0;
    check(unknown_reverse.run()==NBA97_TEXT_UNKNOWN &&
        unknown_reverse.progress.stopped_pc==0x80099304u &&
        unknown_reverse.calls.empty());
    Fixture malformed;*malformed.known(Level)=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc==0x800992d8u);
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x800992d0u);
    Fixture missing;missing.context.memory={&missing.regions[1],1};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x800992d8u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameGraphDebugSetProgress progress{};
    check(nba97_game_graph_debug_set(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_graph_debug_set(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    retail_level_zero();
    diagnostic_path_and_live_epilogue();
    source_quirks_and_unknown_return();
    failures_and_limits();
    std::printf("game_graph_debug_set: %u checks passed\n",checks);
}
