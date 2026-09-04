#include "recovered/game_display_mask_set.h"

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
        std::fprintf(stderr,"game display-mask set check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x20u;
constexpr std::uint32_t CallerRa=0x80029abcu;
constexpr std::uint32_t DebugText=0x800282acu;
constexpr std::uint32_t DebugLevel=0x800c55c2u;
constexpr std::uint32_t DebugPointer=0x800c55bcu;
constexpr std::uint32_t DriverPointer=0x800c55b8u;
constexpr std::uint32_t DriverTable=0x800c5578u;
constexpr std::uint32_t Dispatch=0x8009b16cu;
constexpr std::uint32_t EnvironmentCache=0x800c562cu;
constexpr std::uint32_t MemorySet=0x8009bd78u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameDisplayMaskSetContext context{{regions,2},100,1,EntrySp,CallerRa,
        {0xa0a0a0a0u,0xb1b1b1b1u},io,this};
    Nba97GameDisplayMaskSetProgress progress{};
    std::vector<Nba97GameDisplayMaskSetEvent> calls;
    std::uint32_t final_result=3;
    std::size_t refuse_call=std::numeric_limits<std::size_t>::max();
    std::size_t malformed_call=std::numeric_limits<std::size_t>::max();
    bool final_known=true;
    bool replace_driver_after_debug=false;
    bool rewrite_stack=false;
    bool invalidate_ra=false;
    std::uint32_t gp1_word=0xaaaaaaaa;
    bool visible=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(DebugLevel,0,1);
        put(DebugPointer,0x8009cb2cu);
        put(DriverPointer,DriverTable);
        put(DriverTable+0x10u,Dispatch);
        for(unsigned i=0;i<0x14u;++i)put(EnvironmentCache+i,i,1);
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
        std::uint32_t result=0;
        for(unsigned i=0;i<width;++i)
            result|=std::uint32_t(*byte(address+i))<<(8*i);
        return result;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameDisplayMaskSetEvent* event,
        Nba97GameDisplayMaskSetValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(call==f.refuse_call)return 0;
        if(event->kind==NBA97_GAME_DISPLAY_MASK_DIAGNOSTIC) {
            if(f.replace_driver_after_debug) {
                f.put(DriverPointer,0x800c5700u);
                f.put(0x800c5710u,0x8009b190u);
            }
            *value={0xdeadbeefu,1};
        } else if(event->kind==NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS) {
            if(event->entry!=MemorySet || event->argument_count!=3)return 0;
            for(std::uint32_t i=0;i<event->argument[2];++i)
                f.put(event->argument[0]+i,event->argument[1],1);
            *value={0xfeedfaceu,1};
        } else if(event->kind==NBA97_GAME_DISPLAY_MASK_GPU_CONTROL) {
            f.gp1_word=event->argument[0];
            f.visible=(event->argument[0]&1u)==0;
            *value={f.final_result,static_cast<std::uint8_t>(f.final_known)};
            if(f.rewrite_stack) {
                f.put(event->stack_pointer+0x18u,0x2468ace0u);
                f.put(event->stack_pointer+0x14u,0x13579bdfu);
                f.put(event->stack_pointer+0x10u,0x0badc0deu);
            }
            if(f.invalidate_ra)
                for(unsigned i=0;i<4;++i)
                    *f.known(event->stack_pointer+0x18u+i)=0;
        } else return 0;
        if(call==f.malformed_call)value->known=2;
        return 1;
    }
    int run() {return nba97_game_display_mask_set(&context,&progress);}
};

void startup_enable_path() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.display_enabled && !f.progress.diagnostic_called &&
        !f.progress.environment_cache_clear_called);
    check(f.progress.operations==10 && f.progress.accesses==9 &&
        f.progress.reads==6 && f.progress.stores==3 &&
        f.progress.callbacks_completed==1);
    check(f.progress.requested_mask==1 && f.progress.debug_level==0 &&
        f.progress.driver_table==DriverTable &&
        f.progress.dispatch_target==Dispatch &&
        f.progress.gpu_control_word==0x03000000u);
    check(f.progress.return_v0==3 && f.progress.return_v0_known &&
        f.gp1_word==0x03000000u && f.visible);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.restored_saved_register[0]==0xa0a0a0a0u &&
        f.progress.restored_saved_register[1]==0xb1b1b1b1u);
    check(f.get(FrameSp+0x10u)==0xa0a0a0a0u &&
        f.get(FrameSp+0x14u)==0xb1b1b1b1u &&
        f.get(FrameSp+0x18u)==CallerRa);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_DISPLAY_MASK_GPU_CONTROL &&
        f.calls[0].pc==0x800994d4u && f.calls[0].entry==Dispatch &&
        f.calls[0].argument_count==1 &&
        f.calls[0].argument[0]==0x03000000u &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].return_address==0x800994dcu &&
        f.calls[0].saved_register[0]==1 &&
        f.calls[0].saved_register[1]==DebugLevel);
    for(unsigned i=0;i<0x14u;++i)
        check(f.get(EnvironmentCache+i,1)==i);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.progress.return_v0_known);
}

void disable_and_full_word_boolean_paths() {
    Fixture f;f.context.mask=0;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        !f.progress.display_enabled &&
        f.progress.environment_cache_clear_called && !f.visible);
    check(f.progress.operations==11 && f.progress.accesses==9 &&
        f.progress.reads==6 && f.progress.stores==3 &&
        f.progress.callbacks_completed==2);
    check(f.progress.gpu_control_word==0x03000001u &&
        f.gp1_word==0x03000001u && f.progress.return_v0==3);
    check(f.calls.size()==2 &&
        f.calls[0].kind==NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS &&
        f.calls[0].pc==0x800994acu && f.calls[0].entry==MemorySet &&
        f.calls[0].argument_count==3 &&
        f.calls[0].argument[0]==EnvironmentCache &&
        f.calls[0].argument[1]==UINT32_MAX &&
        f.calls[0].argument[2]==0x14u &&
        f.calls[0].return_address==0x800994b4u &&
        f.calls[1].kind==NBA97_GAME_DISPLAY_MASK_GPU_CONTROL);
    for(unsigned i=0;i<0x14u;++i)
        check(f.get(EnvironmentCache+i,1)==0xffu);

    /* The original tests all 32 bits. Values with a zero low byte are still
       nonzero, unlike SetGraphDebug's byte-truncated argument. */
    for(std::uint32_t mask:{0x100u,0x80000000u,0xffffffffu}) {
        Fixture nonzero;nonzero.context.mask=mask;
        check(nonzero.run()==NBA97_TEXT_COMPLETE &&
            nonzero.progress.display_enabled &&
            nonzero.progress.gpu_control_word==0x03000000u &&
            !nonzero.progress.environment_cache_clear_called &&
            nonzero.calls.size()==1);
    }
}

void diagnostic_live_reload_and_epilogue() {
    Fixture f;f.put(DebugLevel,2,1);f.context.mask=0x12345678u;
    f.replace_driver_after_debug=true;f.rewrite_stack=true;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.diagnostic_called && f.progress.callbacks_completed==2);
    check(f.progress.operations==12 && f.progress.accesses==10 &&
        f.progress.reads==7 && f.progress.stores==3);
    check(f.progress.debug_level==2 &&
        f.progress.debug_callback==0x8009cb2cu &&
        f.progress.driver_table==0x800c5700u &&
        f.progress.dispatch_target==0x8009b190u);
    check(f.calls.size()==2 &&
        f.calls[0].kind==NBA97_GAME_DISPLAY_MASK_DIAGNOSTIC &&
        f.calls[0].pc==0x80099498u &&
        f.calls[0].entry==0x8009cb2cu &&
        f.calls[0].argument_count==2 &&
        f.calls[0].argument[0]==DebugText &&
        f.calls[0].argument[1]==0x12345678u &&
        f.calls[0].return_address==0x800994a0u &&
        f.calls[1].entry==0x8009b190u &&
        f.calls[1].argument[0]==0x03000000u);
    check(f.progress.return_v0==3 &&
        f.progress.restored_return_address==0x2468ace0u &&
        f.progress.restored_saved_register[0]==0x0badc0deu &&
        f.progress.restored_saved_register[1]==0x13579bdfu);

    Fixture ignored;ignored.put(DebugLevel,255,1);ignored.context.mask=1;
    ignored.final_result=0x11223344u;
    check(ignored.run()==NBA97_TEXT_COMPLETE && ignored.calls.size()==2 &&
        ignored.progress.return_v0==0x11223344u);
    Fixture quiet;quiet.put(DebugLevel,1,1);
    check(quiet.run()==NBA97_TEXT_COMPLETE && quiet.calls.size()==1 &&
        !quiet.progress.diagnostic_called);
}

void raw_return_and_unguarded_targets() {
    Fixture opaque;opaque.final_known=false;
    check(opaque.run()==NBA97_TEXT_COMPLETE && opaque.progress.completed &&
        opaque.progress.return_v0==3 && !opaque.progress.return_v0_known);

    Fixture zero;zero.put(DriverTable+0x10u,0);
    check(zero.run()==NBA97_TEXT_COMPLETE && zero.calls.size()==1 &&
        zero.calls[0].entry==0 && zero.progress.dispatch_target==0);

    Fixture unaligned;unaligned.put(DriverTable+0x10u,0x8009b16du);
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x800994d4u &&
        unaligned.progress.stopped_entry==0x8009b16du &&
        unaligned.calls.empty());

    Fixture bad_debug;bad_debug.put(DebugLevel,2,1);
    bad_debug.put(DebugPointer,0x8009cb2du);
    check(bad_debug.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_debug.progress.stopped_pc==0x80099498u &&
        bad_debug.progress.stopped_entry==0x8009cb2du &&
        bad_debug.progress.diagnostic_called);

    Fixture zero_debug;zero_debug.put(DebugLevel,2,1);
    zero_debug.put(DebugPointer,0);
    check(zero_debug.run()==NBA97_TEXT_COMPLETE &&
        zero_debug.calls.size()==2 && zero_debug.calls[0].entry==0 &&
        zero_debug.progress.debug_callback==0);
}

void failures_unknowns_and_limits() {
    const std::uint32_t pcs[]={0x8009945cu,0x80099468u,0x8009946cu,
        0x80099470u,0x800994bcu,0x800994ccu,0x800994d4u,
        0x800994dcu,0x800994e0u,0x800994e4u};
    for(std::size_t budget=0;budget<10;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    Fixture unknown_debug;*unknown_debug.known(DebugLevel)=0;
    check(unknown_debug.run()==NBA97_TEXT_UNKNOWN &&
        unknown_debug.progress.stopped_pc==0x80099470u &&
        unknown_debug.progress.stores==3 && unknown_debug.calls.empty());
    Fixture unknown_table;*unknown_table.known(DriverPointer)=0;
    check(unknown_table.run()==NBA97_TEXT_UNKNOWN &&
        unknown_table.progress.stopped_pc==0x800994bcu);
    Fixture unknown_target;*unknown_target.known(DriverTable+0x10u)=0;
    check(unknown_target.run()==NBA97_TEXT_UNKNOWN &&
        unknown_target.progress.stopped_pc==0x800994ccu);
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x800994d4u);
    Fixture refused;refused.refuse_call=0;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed);
    /* A disable commits its original 20-byte clear before the final device
       boundary. Refusal cannot roll that prefix back. */
    Fixture disable_prefix;disable_prefix.context.mask=0;
    disable_prefix.refuse_call=1;
    check(disable_prefix.run()==NBA97_TEXT_IO_REFUSED &&
        disable_prefix.progress.stopped_pc==0x800994d4u &&
        disable_prefix.progress.environment_cache_clear_called &&
        disable_prefix.progress.callbacks_completed==1 &&
        disable_prefix.calls.size()==2);
    for(unsigned i=0;i<0x14u;++i)
        check(disable_prefix.get(EnvironmentCache+i,1)==0xffu);
    Fixture malformed;malformed.malformed_call=0;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed);
    Fixture invalid_ra;invalid_ra.invalidate_ra=true;
    check(invalid_ra.run()==NBA97_TEXT_UNKNOWN &&
        invalid_ra.progress.return_v0==3 &&
        invalid_ra.progress.stopped_pc==0x800994dcu &&
        invalid_ra.progress.callbacks_completed==1 &&
        !invalid_ra.progress.completed);

    Fixture malformed_memory;*malformed_memory.known(DebugLevel)=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x80099470u);
    Fixture unaligned_stack;unaligned_stack.context.stack_pointer=EntrySp+1u;
    check(unaligned_stack.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_stack.progress.stopped_pc==0x8009945cu);
    Fixture missing_stack;missing_stack.context.stack_pointer=0x90000020u;
    check(missing_stack.run()==NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc==0x8009945cu);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameDisplayMaskSetProgress progress{};
    check(nba97_game_display_mask_set(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture valid;
    check(nba97_game_display_mask_set(&valid.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    startup_enable_path();
    disable_and_full_word_boolean_paths();
    diagnostic_live_reload_and_epilogue();
    raw_return_and_unguarded_targets();
    failures_unknowns_and_limits();
    std::printf("game_display_mask_set: %u checks passed\n",checks);
}
