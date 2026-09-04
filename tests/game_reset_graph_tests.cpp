#include "recovered/game_reset_graph.h"

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
        std::fprintf(stderr,"game reset-graph check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x20u;
constexpr std::uint32_t CallerRa=0x80029a28u;
constexpr std::uint32_t DriverTable=0x800c5578u;
constexpr std::uint32_t DriverPointer=0x800c55b8u;
constexpr std::uint32_t DebugPointer=0x800c55bcu;
constexpr std::uint32_t State=0x800c55c0u;
constexpr std::uint32_t WidthTable=0x800c5640u;
constexpr std::uint32_t HeightTable=0x800c5654u;
constexpr std::uint32_t Diagnostic=0x8009cb2cu;
constexpr std::uint32_t MemorySet=0x8009bd78u;
constexpr std::uint32_t ResetCallback=0x800985dcu;
constexpr std::uint32_t BiosA049=0x8009bda4u;
constexpr std::uint32_t DeviceReset=0x8009b878u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameResetGraphContext context{{regions,2},100,3,EntrySp,CallerRa,
        {0xa0a0a0a0u,0xb1b1b1b1u},io,this};
    Nba97GameResetGraphProgress progress{};
    std::vector<Nba97GameResetGraphEvent> calls;
    std::uint32_t reset_type=0;
    std::uint32_t final_result=0x13579bdfu;
    std::size_t refuse_call=std::numeric_limits<std::size_t>::max();
    std::size_t malformed_call=std::numeric_limits<std::size_t>::max();
    bool reset_known=true;
    bool final_known=true;
    bool rewrite_ra=false;
    bool replace_driver_after_debug=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        put(DriverPointer,DriverTable);
        put(DebugPointer,Diagnostic);
        put(DriverTable+0x34u,DeviceReset);
        put(WidthTable,0x0400u,2);
        put(HeightTable,0x0200u,2);
        put(WidthTable+8u,0x0444u,2);
        put(HeightTable+8u,0x0555u,2);
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
        const Nba97GameResetGraphEvent* event,Nba97GameResetGraphValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(call==f.refuse_call)return 0;
        if(event->entry==MemorySet) {
            if(event->kind!=NBA97_GAME_RESET_GRAPH_DIRECT_CALL ||
                event->argument_count!=3)return 0;
            for(std::uint32_t i=0;i<event->argument[2];++i)
                f.put(event->argument[0]+i,event->argument[1],1);
        } else if(event->pc==0x800990d0u && event->entry==DeviceReset) {
            *value={f.reset_type,static_cast<std::uint8_t>(f.reset_known)};
        } else if(event->pc==0x80099174u) {
            if(f.replace_driver_after_debug) {
                f.put(DriverPointer,0x800c5700u);
                f.put(0x800c5734u,0x8009b274u);
            }
        } else if(event->pc==0x80099190u) {
            *value={f.final_result,static_cast<std::uint8_t>(f.final_known)};
        }
        if(f.rewrite_ra && event->pc==0x80099138u)
            f.put(event->stack_pointer+0x18u,0x2468ace0u);
        if(call==f.malformed_call)value->known=2;
        return 1;
    }
    int run() {return nba97_game_reset_graph(&context,&progress);}
};

void retail_mode_three() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.initialized && !f.progress.debug_reported);
    check(f.progress.operations==23 && f.progress.accesses==16 &&
        f.progress.reads==9 && f.progress.stores==7 &&
        f.progress.callbacks_completed==7);
    check(f.progress.requested_mode==3 && f.progress.masked_mode==3 &&
        f.progress.reset_type==0 && f.progress.driver_table==DriverTable);
    check(f.progress.display_width==0x400 && f.progress.display_height==0x200 &&
        f.progress.display_width_known && f.progress.display_height_known &&
        f.progress.return_v0==0 && f.progress.return_v0_known);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.restored_saved_register[0]==0xa0a0a0a0u &&
        f.progress.restored_saved_register[1]==0xb1b1b1b1u);
    check(f.get(FrameSp+0x10u)==0xa0a0a0a0u &&
        f.get(FrameSp+0x14u)==0xb1b1b1b1u &&
        f.get(FrameSp+0x18u)==CallerRa);
    check(f.get(State)==0x00000100u &&
        f.get(State+4u)==0x02000400u);
    for(std::uint32_t address=State+0x10u;address<State+0x80u;++address)
        check(f.get(address,1)==0xffu);
    check(f.calls.size()==7);
    const std::uint32_t pcs[]={0x80099098u,0x800990a8u,0x800990b0u,
        0x800990c8u,0x800990d0u,0x80099128u,0x80099138u};
    const std::uint32_t entries[]={Diagnostic,MemorySet,ResetCallback,
        BiosA049,DeviceReset,MemorySet,MemorySet};
    for(unsigned i=0;i<7;++i)check(f.calls[i].pc==pcs[i] &&
        f.calls[i].entry==entries[i] &&
        f.calls[i].kind==NBA97_GAME_RESET_GRAPH_DIRECT_CALL &&
        f.calls[i].stack_pointer==FrameSp &&
        f.calls[i].return_address==pcs[i]+8u);
    check(f.calls[0].argument_count==3 &&
        f.calls[0].argument[0]==0x80028204u &&
        f.calls[0].argument[1]==DriverTable &&
        f.calls[0].argument[2]==State &&
        f.calls[0].saved_register[0]==State &&
        f.calls[0].saved_register[1]==3);
    check(f.calls[1].argument[0]==State && !f.calls[1].argument[1] &&
        f.calls[1].argument[2]==0x80u);
    check(!f.calls[2].argument_count);
    check(f.calls[3].argument_count==1 &&
        f.calls[3].argument[0]==0x000c5578u);
    check(f.calls[4].argument_count==1 && f.calls[4].argument[0]==1);
    check(f.calls[5].argument[0]==State+0x10u &&
        f.calls[5].argument[1]==UINT32_MAX &&
        f.calls[5].argument[2]==0x5cu);
    check(f.calls[6].argument[0]==State+0x6cu &&
        f.calls[6].argument[1]==UINT32_MAX &&
        f.calls[6].argument[2]==0x14u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.get(State+4u)==0x02000400u);
}

void mode_zero_and_source_quirks() {
    Fixture mode_zero;mode_zero.context.mode=0;mode_zero.reset_type=2;
    check(mode_zero.run()==NBA97_TEXT_COMPLETE &&
        mode_zero.progress.masked_mode==0 && mode_zero.progress.reset_type==2 &&
        mode_zero.calls[4].argument[0]==0 &&
        mode_zero.progress.display_width==0x444 &&
        mode_zero.progress.display_height==0x555 &&
        mode_zero.progress.return_v0==2);

    /* Original aliases every input with the same low three bits. */
    Fixture masked;masked.context.mode=0xfffffffbu;
    check(masked.run()==NBA97_TEXT_COMPLETE && masked.progress.initialized &&
        masked.progress.masked_mode==3 && masked.calls[4].argument[0]==1);

    /* Keep the retail unchecked byte index instead of repairing it. */
    Fixture unchecked;unchecked.reset_type=0xffu;
    unchecked.put(WidthTable+0x3fcu,0x1357u,2);
    unchecked.put(HeightTable+0x3fcu,0x2468u,2);
    check(unchecked.run()==NBA97_TEXT_COMPLETE &&
        unchecked.progress.reset_type==0xffu &&
        unchecked.progress.display_width==0x1357u &&
        unchecked.progress.display_height==0x2468u &&
        unchecked.get(State+4u,2)==0x1357u &&
        unchecked.get(State+6u,2)==0x2468u);

    Fixture live;live.put(DriverPointer,0x80abcdefu);live.rewrite_ra=true;
    check(live.run()==NBA97_TEXT_COMPLETE &&
        live.calls[3].argument[0]==0x00abcdefu &&
        live.progress.restored_return_address==0x2468ace0u);

    Fixture unknown_width;
    *unknown_width.known(WidthTable)=0;
    check(unknown_width.run()==NBA97_TEXT_COMPLETE &&
        !unknown_width.progress.display_width_known &&
        unknown_width.progress.display_height_known &&
        !*unknown_width.known(State+4u) &&
        !*unknown_width.known(State+5u));
}

void driver_paths() {
    Fixture quiet;quiet.context.mode=1;quiet.put(State+2u,1,1);
    check(quiet.run()==NBA97_TEXT_COMPLETE && quiet.progress.completed &&
        !quiet.progress.initialized && !quiet.progress.debug_reported);
    check(quiet.progress.operations==10 && quiet.progress.accesses==9 &&
        quiet.progress.reads==6 && quiet.progress.stores==3 &&
        quiet.progress.callbacks_completed==1);
    check(quiet.calls.size()==1 && quiet.calls[0].pc==0x80099190u &&
        quiet.calls[0].entry==DeviceReset &&
        quiet.calls[0].kind==NBA97_GAME_RESET_GRAPH_INDIRECT_CALL &&
        quiet.calls[0].argument_count==1 && quiet.calls[0].argument[0]==1 &&
        quiet.progress.driver_reset_target==DeviceReset &&
        quiet.progress.return_v0==quiet.final_result &&
        quiet.progress.return_v0_known);

    Fixture debug;debug.context.mode=0xfffffffau;debug.put(State+2u,2,1);
    debug.replace_driver_after_debug=true;
    check(debug.run()==NBA97_TEXT_COMPLETE && debug.progress.completed &&
        debug.progress.masked_mode==2 && debug.progress.debug_reported);
    check(debug.progress.operations==12 && debug.progress.accesses==10 &&
        debug.progress.reads==7 && debug.progress.stores==3 &&
        debug.progress.callbacks_completed==2 && debug.calls.size()==2);
    check(debug.calls[0].pc==0x80099174u &&
        debug.calls[0].entry==Diagnostic &&
        debug.calls[0].kind==NBA97_GAME_RESET_GRAPH_INDIRECT_CALL &&
        debug.calls[0].argument_count==2 &&
        debug.calls[0].argument[0]==0x80028224u &&
        debug.calls[0].argument[1]==0xfffffffau);
    check(debug.calls[1].pc==0x80099190u &&
        debug.calls[1].entry==0x8009b274u &&
        debug.progress.driver_table==0x800c5700u &&
        debug.progress.driver_reset_target==0x8009b274u);

    Fixture opaque;opaque.context.mode=1;opaque.put(State+2u,0,1);
    opaque.final_known=false;
    check(opaque.run()==NBA97_TEXT_COMPLETE && opaque.progress.completed &&
        opaque.progress.return_v0==opaque.final_result &&
        !opaque.progress.return_v0_known);
}

void failures_and_limits() {
    const std::uint32_t pcs[]={0x80099060u,0x80099068u,0x80099070u,
        0x80099098u,0x800990a8u,0x800990b0u,0x800990c0u,
        0x800990c8u,0x800990d0u,0x800990dcu,0x800990e0u,
        0x800990ecu,0x800990fcu,0x80099100u,0x80099110u,
        0x8009911cu,0x80099124u,0x80099128u,0x80099138u,
        0x80099140u,0x80099198u,0x8009919cu,0x800991a0u};
    for(std::size_t budget=0;budget<23;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x80099098u);
    Fixture refused;refused.refuse_call=3;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==4 &&
        refused.progress.callbacks_completed==3);
    Fixture malformed;malformed.malformed_call=4;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==5 &&
        malformed.progress.callbacks_completed==4);
    Fixture opaque_reset;opaque_reset.reset_known=false;
    check(opaque_reset.run()==NBA97_TEXT_UNKNOWN &&
        opaque_reset.progress.stopped_pc==0x800990e0u &&
        !*opaque_reset.known(State));
    Fixture bad_target;bad_target.context.mode=1;bad_target.put(State+2u,0,1);
    bad_target.put(DriverTable+0x34u,0x8009b879u);
    check(bad_target.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_target.progress.stopped_pc==0x80099190u &&
        bad_target.progress.stopped_entry==0x8009b879u);
    Fixture unknown_debug;unknown_debug.context.mode=1;
    *unknown_debug.known(State+2u)=0;
    check(unknown_debug.run()==NBA97_TEXT_UNKNOWN &&
        unknown_debug.progress.stopped_pc==0x80099150u);
    Fixture malformed_memory;*malformed_memory.known(DriverPointer)=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x800990c0u);
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80099060u);
    Fixture missing;missing.context.stack_pointer=0x90000020u;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80099060u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameResetGraphProgress progress{};
    check(nba97_game_reset_graph(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_reset_graph(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    retail_mode_three();
    mode_zero_and_source_quirks();
    driver_paths();
    failures_and_limits();
    std::printf("game_reset_graph: %u checks passed\n",checks);
}
