#include "recovered/game_vblank_initialize.h"

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
        std::fprintf(stderr,"game vblank-initialize check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x20u;
constexpr std::uint32_t CallerRa=0x80029a40u;
constexpr std::uint32_t IncomingFp=0xf3f3f3f3u;
constexpr std::uint32_t Gp=0x800d79c8u;
constexpr std::uint32_t Slots=0x800d6e0cu;
constexpr std::uint32_t Handler=0x800a450cu;
constexpr std::uint32_t Counter=0xf2000003u;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameVblankInitializeContext context{{regions,2},100,EntrySp,
        CallerRa,IncomingFp,Gp,io,this};
    Nba97GameVblankInitializeProgress progress{};
    std::vector<Nba97GameVblankInitializeEvent> calls;
    std::size_t refuse_call=std::numeric_limits<std::size_t>::max();
    std::size_t malformed_call=std::numeric_limits<std::size_t>::max();
    bool mutate_saved_stack=false;
    bool unknown_saved_ra=false;
    bool set_rcnt_rejected=false;
    bool vblank_unmasked=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        for(unsigned i=0;i<8;++i)put(Slots+i*4u,0x11110000u+i);
        put(0x800d7a88u,0xaaaaaaaau);
        put(0x800d7afcu,0xbbbbbbbbu);
        put(0x800d7b00u,0xccccccccu);
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
        const Nba97GameVblankInitializeEvent* event,
        Nba97GameVblankInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(call==f.refuse_call)return 0;
        if(call==f.malformed_call) {value->known=2;return 1;}
        value->word=0x90000000u+static_cast<std::uint32_t>(call);
        value->known=1;
        switch(event->entry) {
        case 0x800a4830u:
            f.put(0x800d6e2cu,event->global_pointer);
            break;
        case 0x800983b4u:
            f.set_rcnt_rejected=true;
            value->word=0; /* PsyQ rejects low-half counter index 3. */
            break;
        case 0x80098488u:
            f.vblank_unmasked=true; /* Its store precedes its false return. */
            value->word=0;
            break;
        case 0x800a3e48u:
            f.put(0x800d7a88u,0);f.put(0x800d7afcu,0);f.put(0x800d7b00u,0);
            value->word=0;
            if(f.mutate_saved_stack) {
                f.put(event->stack_pointer+0x1cu,0x2468ace0u);
                f.put(event->stack_pointer+0x18u,0x13579bdfu);
            }
            if(f.unknown_saved_ra)
                for(unsigned i=0;i<4;++i)*f.known(event->stack_pointer+0x1cu+i)=0;
            break;
        default:break;
        }
        return 1;
    }
    int run() {return nba97_game_vblank_initialize(&context,&progress);}
};

void retail_startup() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==54 && f.progress.accesses==46 &&
        f.progress.reads==27 && f.progress.stores==19 &&
        f.progress.callbacks_completed==8);
    check(f.progress.callback_slots_cleared==8 &&
        f.progress.interrupt_handler==Handler &&
        f.progress.root_counter_spec==Counter &&
        f.progress.root_counter_target==1 &&
        f.progress.root_counter_mode==0x1000u);
    check(f.progress.set_rcnt_return==0 &&
        f.progress.set_rcnt_return_known &&
        f.progress.start_rcnt_return==0 &&
        f.progress.start_rcnt_return_known &&
        f.progress.return_v0==0 && f.progress.return_v0_known);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.global_pointer==Gp &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.restored_frame_pointer==IncomingFp);
    check(f.get(FrameSp+0x1cu)==CallerRa &&
        f.get(FrameSp+0x18u)==IncomingFp && f.get(FrameSp+0x10u)==8);
    for(unsigned i=0;i<8;++i)check(f.get(Slots+i*4u)==0);
    check(f.get(0x800d6e2cu)==Gp && f.get(0x800d7a88u)==0 &&
        f.get(0x800d7afcu)==0 && f.get(0x800d7b00u)==0);
    check(f.set_rcnt_rejected && f.vblank_unmasked);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    const std::uint32_t pcs[]={0x800a43f8u,0x800a4460u,0x800a4468u,
        0x800a447cu,0x800a4494u,0x800a44a4u,0x800a44acu,0x800a44b4u};
    const std::uint32_t entries[]={0x800a4830u,0x800994f4u,0x80098394u,
        0x8009860cu,0x800983b4u,0x80098488u,0x80098594u,0x800a3e48u};
    for(unsigned i=0;i<8;++i) {
        const auto& call=f.calls[i];
        check(call.pc==pcs[i] && call.entry==entries[i] &&
            call.return_address==pcs[i]+8u && call.stack_pointer==FrameSp &&
            call.frame_pointer==FrameSp && call.global_pointer==Gp);
    }
    check(f.calls[0].argument_count==0 && f.calls[1].argument_count==1 &&
        f.calls[1].argument[0]==0 && f.calls[2].argument_count==0);
    check(f.calls[3].argument_count==2 && f.calls[3].argument[0]==0 &&
        f.calls[3].argument[1]==Handler);
    check(f.calls[4].argument_count==3 && f.calls[4].argument[0]==Counter &&
        f.calls[4].argument[1]==1 && f.calls[4].argument[2]==0x1000u);
    check(f.calls[5].argument_count==1 && f.calls[5].argument[0]==Counter &&
        f.calls[6].argument_count==0 && f.calls[7].argument_count==0);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.progress.completed);
}

void original_quirks_and_live_epilogue() {
    Fixture f;f.mutate_saved_stack=true;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.restored_return_address==0x2468ace0u &&
        f.progress.restored_frame_pointer==0x13579bdfu);
    /* Large successful child values prove all pre-final raw returns are
       ignored; counter-3 service failures likewise do not abort startup. */
    check(f.calls.size()==8 && f.set_rcnt_rejected && f.vblank_unmasked &&
        f.progress.return_v0==0);

    for(std::size_t refused=0;refused<8;++refused) {
        Fixture partial;partial.refuse_call=refused;
        check(partial.run()==NBA97_TEXT_IO_REFUSED &&
            partial.calls.size()==refused+1 &&
            partial.progress.callbacks_completed==refused &&
            !partial.progress.completed);
        /* The source is prefix-committing: callback words were already
           cleared before any of the platform calls can refuse. */
        if(refused>0)
            for(unsigned i=0;i<8;++i)check(partial.get(Slots+i*4u)==0);
    }
}

void limits_and_failures() {
    std::vector<std::uint32_t> pcs={0x800a43ecu,0x800a43f0u,0x800a43f8u,
        0x800a4400u};
    for(unsigned i=0;i<8;++i) {
        pcs.push_back(0x800a4404u);pcs.push_back(0x800a4420u);
        pcs.push_back(0x800a443cu);
        pcs.push_back(0x800a4440u);pcs.push_back(0x800a4450u);
    }
    pcs.push_back(0x800a4404u);
    for(auto pc:{0x800a4460u,0x800a4468u,0x800a447cu,0x800a4494u,
                 0x800a44a4u,0x800a44acu,0x800a44b4u,0x800a44c0u,
                 0x800a44c4u})pcs.push_back(pc);
    check(pcs.size()==54);
    for(std::size_t budget=0;budget<pcs.size();++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x800a43f8u &&
        no_io.progress.stopped_entry==0x800a4830u &&
        no_io.progress.stores==2);
    Fixture malformed_return;malformed_return.malformed_call=0;
    check(malformed_return.run()==NBA97_TEXT_ARGUMENT &&
        malformed_return.progress.stopped_pc==0x800a43f8u);
    Fixture unknown_stack;unknown_stack.unknown_saved_ra=true;
    check(unknown_stack.run()==NBA97_TEXT_UNKNOWN &&
        unknown_stack.progress.stopped_pc==0x800a44c0u &&
        unknown_stack.progress.stopped_address==FrameSp+0x1cu);
    Fixture malformed_memory;*malformed_memory.known(Slots)=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x800a443cu);
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x800a43ecu);
    Fixture missing;missing.context.memory={&missing.regions[1],1};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x800a443cu);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.regions[0],overlap.regions[0]};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.regions[0].size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.regions[0].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameVblankInitializeProgress progress{};
    check(nba97_game_vblank_initialize(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_vblank_initialize(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    retail_startup();
    original_quirks_and_live_epilogue();
    limits_and_failures();
    std::printf("game_vblank_initialize: %u checks passed\n",checks);
}
