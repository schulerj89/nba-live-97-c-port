#include "recovered/game_video_environment_initialize.h"

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
        std::fprintf(stderr,"game video-environment check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x38u;
constexpr std::uint32_t CallerRa=0x80029a74u;
constexpr std::uint32_t Gp=0x800d79c8u;
constexpr std::uint32_t Draw0=0x80021eecu;
constexpr std::uint32_t Draw1=0x80021f48u;
constexpr std::uint32_t Draw2=0x80021fa4u;
constexpr std::uint32_t Draw3=0x80022000u;
constexpr std::uint32_t Disp0=0x8002205cu;
constexpr std::uint32_t Disp1=0x80022070u;
constexpr std::uint32_t Saved[6]={0x10101010u,0x21212121u,0x32323232u,
    0x43434343u,0x54545454u,0x65656565u};

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameVideoEnvironmentInitializeContext context{};
    Nba97GameVideoEnvironmentInitializeProgress progress{};
    std::vector<Nba97GameVideoEnvironmentInitializeEvent> calls;
    std::size_t refuse_call=std::numeric_limits<std::size_t>::max();
    std::size_t malformed_call=std::numeric_limits<std::size_t>::max();
    std::size_t unknown_return_call=std::numeric_limits<std::size_t>::max();
    bool perform_children=true;
    bool rewrite_saved_on_sync=false;
    bool unknown_ra_on_sync=false;
    std::uint32_t active_display=0;
    std::uint32_t active_draw=0;
    unsigned display_installs=0;
    unsigned draw_installs=0;
    bool synchronized=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        context.memory={regions,2};
        context.operation_budget=100;
        context.background_mode=0;
        context.stack_pointer=EntrySp;
        context.return_address=CallerRa;
        for(unsigned i=0;i<6;++i)context.saved_register[i]=Saved[i];
        context.global_pointer=Gp;
        context.io=io;
        context.user=this;
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
                return region.known?region.known+(address-region.base):nullptr;
        return nullptr;
    }
    void put(std::uint32_t address,std::uint32_t value,unsigned width) {
        for(unsigned i=0;i<width;++i) {
            auto* target=byte(address+i);
            check(target!=nullptr);
            *target=std::uint8_t(value>>(8*i));
            if(auto* mask=known(address+i))*mask=1;
        }
    }
    std::uint32_t get(std::uint32_t address,unsigned width=4) const {
        std::uint32_t value=0;
        for(const auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)+width<=region.size) {
                const auto* data=region.data+(address-region.base);
                for(unsigned i=0;i<width;++i)
                    value|=std::uint32_t(data[i])<<(8*i);
                return value;
            }
        check(false);return 0;
    }
    void set_def_disp(const Nba97GameVideoEnvironmentInitializeEvent& e) {
        const auto p=e.argument[0];
        put(p+0,e.argument[1],2);put(p+2,e.argument[2],2);
        put(p+4,e.argument[3],2);put(p+6,e.argument[4],2);
        for(unsigned offset=8;offset<16;offset+=2)put(p+offset,0,2);
        for(unsigned offset=16;offset<20;++offset)put(p+offset,0,1);
    }
    void set_def_draw(const Nba97GameVideoEnvironmentInitializeEvent& e) {
        const auto p=e.argument[0];
        put(p+0,e.argument[1],2);put(p+2,e.argument[2],2);
        put(p+4,e.argument[3],2);put(p+6,e.argument[4],2);
        put(p+8,e.argument[1],2);put(p+10,e.argument[2],2);
        for(unsigned offset=12;offset<20;offset+=2)put(p+offset,0,2);
        put(p+20,10,2);put(p+22,1,1);put(p+23,e.argument[4]<0x101u,1);
        put(p+24,0,1);put(p+25,0,1);put(p+26,0,1);put(p+27,0,1);
    }
    bool expected(const Nba97GameVideoEnvironmentInitializeEvent& e,
        std::size_t call) {
        static constexpr std::uint32_t pcs[9]={0x80029f60u,0x80029f7cu,
            0x80029f9cu,0x80029fb8u,0x8002a040u,0x8002a048u,
            0x8002a050u,0x8002a058u,0x8002a060u};
        static constexpr std::uint32_t entries[9]={0x8009cad0u,0x8009cad0u,
            0x8009ca00u,0x8009ca00u,0x80099ca4u,0x80099accu,
            0x80099ca4u,0x80099accu,0x800994f4u};
        static constexpr std::uint8_t kinds[9]={
            NBA97_GAME_VIDEO_SET_DEF_DISP_ENV,
            NBA97_GAME_VIDEO_SET_DEF_DISP_ENV,
            NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV,
            NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV,
            NBA97_GAME_VIDEO_PUT_DISP_ENV,NBA97_GAME_VIDEO_PUT_DRAW_ENV,
            NBA97_GAME_VIDEO_PUT_DISP_ENV,NBA97_GAME_VIDEO_PUT_DRAW_ENV,
            NBA97_GAME_VIDEO_DRAW_SYNC};
        if(call>=9 || e.pc!=pcs[call] || e.entry!=entries[call] ||
           e.kind!=kinds[call] || e.stack_pointer!=FrameSp ||
           e.global_pointer!=Gp || e.return_address!=e.pc+8u)
            return false;
        if(call<4) {
            const std::uint32_t pointer[4]={Disp0,Disp1,Draw0,Draw1};
            const std::uint32_t y[4]={0x100u,0,0,0x100u};
            return e.argument_count==5 && e.argument[0]==pointer[call] &&
                e.argument[1]==0 && e.argument[2]==y[call] &&
                e.argument[3]==0x200u && e.argument[4]==0xf0u;
        }
        const std::uint32_t pointer[4]={Disp0,Draw0,Disp1,Draw1};
        if(call<8)
            return e.argument_count==1 && e.argument[0]==pointer[call-4];
        return e.argument_count==1 && e.argument[0]==0;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameVideoEnvironmentInitializeEvent* event,
        Nba97GameVideoEnvironmentInitializeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto call=f.calls.size();
        f.calls.push_back(*event);
        if(!f.expected(*event,call))return 0;
        if(call==f.refuse_call)return 0;
        if(f.perform_children) {
            if(event->kind==NBA97_GAME_VIDEO_SET_DEF_DISP_ENV)
                f.set_def_disp(*event);
            else if(event->kind==NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV)
                f.set_def_draw(*event);
            else if(event->kind==NBA97_GAME_VIDEO_PUT_DISP_ENV) {
                f.active_display=event->argument[0];++f.display_installs;
            } else if(event->kind==NBA97_GAME_VIDEO_PUT_DRAW_ENV) {
                f.active_draw=event->argument[0];++f.draw_installs;
            } else {
                f.synchronized=true;
                if(f.rewrite_saved_on_sync) {
                    f.put(event->stack_pointer+0x30u,0xa0a0a0a0u,4);
                    f.put(event->stack_pointer+0x18u,0xb0b0b0b0u,4);
                    f.put(event->stack_pointer+0x1cu,0xb1b1b1b1u,4);
                    f.put(event->stack_pointer+0x20u,0xb2b2b2b2u,4);
                    f.put(event->stack_pointer+0x24u,0xb3b3b3b3u,4);
                    f.put(event->stack_pointer+0x28u,0xb4b4b4b4u,4);
                    f.put(event->stack_pointer+0x2cu,0xb5b5b5b5u,4);
                }
                if(f.unknown_ra_on_sync)
                    for(unsigned i=0;i<4;++i)
                        *f.known(event->stack_pointer+0x30u+i)=0;
            }
        }
        value->word=0x90000000u+static_cast<std::uint32_t>(call);
        value->known=call==f.unknown_return_call?0:1;
        if(call==f.malformed_call)value->known=2;
        return 1;
    }
    int run() {
        return nba97_game_video_environment_initialize(&context,&progress);
    }
};

void retail_double_buffer() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==44 && f.progress.accesses==35 &&
        f.progress.reads==7 && f.progress.stores==28 &&
        f.progress.callbacks_completed==9 &&
        f.progress.direct_control_bytes_written==16);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp && f.progress.global_pointer==Gp &&
        f.progress.requested_background_mode==0 &&
        f.progress.background_byte==0 &&
        f.progress.display_environment[0]==Disp0 &&
        f.progress.display_environment[1]==Disp1 &&
        f.progress.draw_environment[0]==Draw0 &&
        f.progress.draw_environment[1]==Draw1);
    check(f.progress.restored_return_address==CallerRa &&
        f.progress.return_v0==0x90000008u && f.progress.return_v0_known);
    for(unsigned i=0;i<6;++i)
        check(f.progress.restored_saved_register[i]==Saved[i]);
}

void environments_and_call_order() {
    Fixture f;
    f.context.background_mode=1;
    check(f.run()==NBA97_TEXT_COMPLETE);
    check(f.get(Disp0+0,2)==0 && f.get(Disp0+2,2)==0x100u &&
        f.get(Disp0+4,2)==0x200u && f.get(Disp0+6,2)==0xf0u);
    check(f.get(Disp1+0,2)==0 && f.get(Disp1+2,2)==0 &&
        f.get(Disp1+4,2)==0x200u && f.get(Disp1+6,2)==0xf0u);
    for(auto p:{Disp0,Disp1})
        check(f.get(p+8)==0 && f.get(p+12)==0 &&
            f.get(p+16,1)==0 && f.get(p+17,1)==0 &&
            f.get(p+18,1)==0 && f.get(p+19,1)==0);
    check(f.get(Draw0+0,2)==0 && f.get(Draw0+2,2)==0 &&
        f.get(Draw0+4,2)==0x200u && f.get(Draw0+6,2)==0xf0u &&
        f.get(Draw0+8,2)==0 && f.get(Draw0+10,2)==0);
    check(f.get(Draw1+0,2)==0 && f.get(Draw1+2,2)==0x100u &&
        f.get(Draw1+4,2)==0x200u && f.get(Draw1+6,2)==0xf0u &&
        f.get(Draw1+8,2)==0 && f.get(Draw1+10,2)==0x100u);
    for(auto p:{Draw0,Draw1})
        check(f.get(p+12)==0 && f.get(p+16)==0 &&
            f.get(p+20,2)==10 && f.get(p+22,1)==0 &&
            f.get(p+23,1)==1 && f.get(p+24,1)==1 &&
            f.get(p+25,1)==0 && f.get(p+26,1)==0 &&
            f.get(p+27,1)==0);
    /* Source oddity: only dtd/isbg change in the two SetDef-untouched rows. */
    for(auto p:{Draw2,Draw3})
        check(f.get(p+21,1)==0xcdu && f.get(p+22,1)==0 &&
            f.get(p+23,1)==0xcdu && f.get(p+24,1)==1 &&
            f.get(p+25,1)==0xcdu);
    check(f.get(0x8001ede8u)==0 && f.active_display==Disp1 &&
        f.active_draw==Draw1 && f.display_installs==2 &&
        f.draw_installs==2 && f.synchronized);
    check(f.calls.size()==9 && f.get(FrameSp+0x10u)==0xf0u);
    check(f.calls[0].saved_register[0]==0xf0u &&
        f.calls[0].saved_register[1]==1 &&
        f.calls[0].saved_register[2]==Disp0 &&
        f.calls[0].saved_register[3]==Saved[3] &&
        f.calls[0].saved_register[4]==Saved[4] &&
        f.calls[0].saved_register[5]==Saved[5]);
    check(f.calls[1].saved_register[5]==Disp1 &&
        f.calls[2].saved_register[3]==Draw0 &&
        f.calls[3].saved_register[4]==Draw1);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);
}

void truncation_and_live_epilogue() {
    Fixture f;f.context.background_mode=0x123456abu;
    f.unknown_return_call=0;f.rewrite_saved_on_sync=true;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.background_byte==0xabu);
    for(auto p:{Draw0,Draw1,Draw2,Draw3})check(f.get(p+24,1)==0xabu);
    check(f.progress.restored_return_address==0xa0a0a0a0u);
    for(unsigned i=0;i<6;++i)
        check(f.progress.restored_saved_register[i]==0xb0b0b0b0u+i*0x01010101u);
    check(f.progress.return_v0==0x90000008u && f.progress.return_v0_known);

    Fixture unknown_last;unknown_last.unknown_return_call=8;
    check(unknown_last.run()==NBA97_TEXT_COMPLETE &&
        unknown_last.progress.completed &&
        unknown_last.progress.return_v0==0x90000008u &&
        !unknown_last.progress.return_v0_known);

    Fixture unknown_ra;unknown_ra.unknown_ra_on_sync=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations==38 && unknown_ra.progress.stores==28 &&
        !unknown_ra.progress.reads &&
        unknown_ra.progress.stopped_pc==0x8002a070u &&
        unknown_ra.progress.stopped_address==FrameSp+0x30u);
}

void limits_and_refusals() {
    constexpr std::array<std::uint32_t,44> pcs={
        0x80029f24u,0x80029f2cu,0x80029f48u,0x80029f50u,
        0x80029f54u,0x80029f58u,0x80029f5cu,
        0x80029f64u,0x80029f60u,0x80029f80u,0x80029f7cu,
        0x80029fa0u,0x80029f9cu,0x80029fbcu,0x80029fb8u,
        0x80029fc4u,0x80029fccu,0x80029fd4u,0x80029fdcu,
        0x80029fe4u,0x80029fecu,0x80029ff4u,0x80029ffcu,
        0x8002a004u,0x8002a00cu,0x8002a014u,0x8002a01cu,
        0x8002a024u,0x8002a02cu,0x8002a034u,0x8002a03cu,
        0x8002a040u,0x8002a048u,0x8002a050u,0x8002a058u,
        0x8002a060u,0x8002a06cu,0x8002a070u,0x8002a074u,
        0x8002a078u,0x8002a07cu,0x8002a080u,0x8002a084u,
        0x8002a088u};
    for(std::size_t budget=0;budget<pcs.size();++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT &&
            f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
    }
    for(std::size_t refused=0;refused<9;++refused) {
        Fixture f;f.refuse_call=refused;
        check(f.run()==NBA97_TEXT_IO_REFUSED && f.calls.size()==refused+1 &&
            f.progress.callbacks_completed==refused && !f.progress.completed);
    }
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc==0x80029f60u &&
        no_io.progress.stopped_entry==0x8009cad0u &&
        no_io.progress.stores==8);
    Fixture malformed;malformed.malformed_call=3;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==4 &&
        malformed.progress.callbacks_completed==3 &&
        malformed.progress.stopped_pc==0x80029fb8u);
}

void memory_and_arguments() {
    Fixture without_masks;without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE);
    Fixture malformed_stack;*malformed_stack.known(FrameSp+0x1cu)=2;
    check(malformed_stack.run()==NBA97_TEXT_ARGUMENT &&
        malformed_stack.progress.stopped_pc==0x80029f24u);
    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80029f24u);
    Fixture missing;missing.perform_children=false;
    missing.context.memory={&missing.regions[1],1};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80029fc4u &&
        missing.progress.callbacks_completed==4);
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
    Nba97GameVideoEnvironmentInitializeProgress progress{};
    check(nba97_game_video_environment_initialize(nullptr,&progress)==
        NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_video_environment_initialize(&f.context,nullptr)==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    retail_double_buffer();
    environments_and_call_order();
    truncation_and_live_epilogue();
    limits_and_refusals();
    memory_and_arguments();
    std::printf("game_video_environment_initialize: %u checks passed\n",checks);
}
