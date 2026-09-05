#include "recovered/game_loading_screen.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game loading-screen check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffd0u;
constexpr std::uint32_t FrameSp = EntrySp - 0x28u;
constexpr std::uint32_t Resource = 0x80123400u;
constexpr std::uint32_t Image = 0x80134560u;

struct Fixture {
    std::array<std::uint8_t,0x100> stack{},known{};
    Nba97GameTextRegion region{Stack,stack.data(),known.data(),stack.size()};
    Nba97GameLoadingScreenContext context{{&region,1},100,EntrySp,
        0x80029aecu,{0x11112222u,0x33334444u},0x800d79c8u,io,this};
    Nba97GameLoadingScreenProgress progress{};
    std::vector<Nba97GameLoadingScreenEvent> calls;
    std::size_t refuse=static_cast<std::size_t>(-1);
    std::size_t malformed=static_cast<std::size_t>(-1);
    std::uint32_t resource=Resource;
    std::uint8_t resource_known=1;
    std::uint32_t image=Image;
    std::uint8_t image_known=1;
    bool mutate_epilogue=false;
    bool unknown_epilogue=false;

    Fixture() { stack.fill(0xcd);known.fill(1); }
    void put(std::uint32_t address,std::uint32_t value) {
        const auto offset=address-Stack;
        for(unsigned i=0;i<4;++i) {
            stack[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
            known[offset+i]=1;
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto offset=address-Stack;std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(stack[offset+i])<<(i*8u);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameLoadingScreenEvent* event,
        Nba97GameLoadingScreenValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto index=f.calls.size();
        f.calls.push_back(*event);
        if(index==f.refuse)return 0;
        *value={0,1};
        if(event->kind==NBA97_GAME_LOADING_SCREEN_LOAD_RESOURCE)
            *value={f.resource,f.resource_known};
        else if(event->kind==NBA97_GAME_LOADING_SCREEN_FIND_IMAGE)
            *value={f.image,f.image_known};
        else if(event->kind==NBA97_GAME_LOADING_SCREEN_RELEASE_RESOURCE) {
            *value={0x89abcdefu,1};
            if(f.mutate_epilogue) {
                f.put(FrameSp+0x20u,0x55667788u);
                f.put(FrameSp+0x1cu,0xa1a2a3a4u);
                f.put(FrameSp+0x18u,0xb1b2b3b4u);
            }
            if(f.unknown_epilogue)
                f.known[FrameSp+0x20u-Stack]=0;
        }
        if(index==f.malformed)value->known=2;
        return 1;
    }
    int run() {return nba97_game_loading_screen(&context,&progress);}
};

void ordinary_path() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==16 && f.progress.accesses==6 &&
        f.progress.reads==3 && f.progress.stores==3 &&
        f.progress.callbacks_completed==10);
    check(f.progress.load_calls==1 && f.progress.lookup_calls==1 &&
        f.progress.draw_sync_calls==4 && f.progress.upload_calls==3 &&
        f.progress.release_calls==1);
    check(f.progress.resource_loaded && f.progress.image_lookup_completed &&
        f.progress.resolved_image_known &&
        !f.progress.skipped_for_null_resource &&
        f.progress.loaded_resource==Resource &&
        f.progress.resolved_image==Image &&
        f.progress.return_v0==0x89abcdefu &&
        f.progress.return_v0_known);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.global_pointer==0x800d79c8u &&
        f.progress.restored_return_address==0x80029aecu &&
        f.progress.restored_saved_register[0]==0x11112222u &&
        f.progress.restored_saved_register[1]==0x33334444u);
    check(f.get(FrameSp+0x20u)==0x80029aecu &&
        f.get(FrameSp+0x1cu)==0x33334444u &&
        f.get(FrameSp+0x18u)==0x11112222u);

    static constexpr std::uint32_t pcs[10]={0x80029e70u,0x80029e8cu,
        0x80029e98u,0x80029eb0u,0x80029eb8u,0x80029ed0u,
        0x80029ed8u,0x80029ef0u,0x80029ef8u,0x80029f00u};
    static constexpr std::uint32_t entries[10]={0x80029bfcu,0x800a5478u,
        0x800994f4u,0x800946b8u,0x800994f4u,0x800946b8u,
        0x800994f4u,0x800946b8u,0x800994f4u,0x80090698u};
    check(f.calls.size()==10);
    for(unsigned i=0;i<10;++i)
        check(f.calls[i].pc==pcs[i] && f.calls[i].entry==entries[i] &&
            f.calls[i].stack_pointer==FrameSp &&
            f.calls[i].global_pointer==0x800d79c8u &&
            f.calls[i].return_address==pcs[i]+8u &&
            f.calls[i].saved_register_known[0] &&
            f.calls[i].saved_register_known[1]);
    check(f.calls[0].kind==NBA97_GAME_LOADING_SCREEN_LOAD_RESOURCE &&
        f.calls[0].argument_count==2 &&
        f.calls[0].argument[0]==0x800247f8u && f.calls[0].argument[1]==0 &&
        f.calls[0].saved_register[0]==0x11112222u &&
        f.calls[0].saved_register[1]==0x33334444u);
    check(f.calls[1].kind==NBA97_GAME_LOADING_SCREEN_FIND_IMAGE &&
        f.calls[1].argument_count==2 && f.calls[1].argument[0]==Resource &&
        f.calls[1].argument[1]==0x80024808u &&
        f.calls[1].saved_register[1]==Resource);
    static constexpr std::uint32_t upload_index[3]={3,5,7};
    static constexpr std::uint32_t x[3]={0,0,0x200u};
    static constexpr std::uint32_t y[3]={0,0x100u,0};
    for(unsigned i=0;i<3;++i) {
        const auto& event=f.calls[upload_index[i]];
        check(event.kind==NBA97_GAME_LOADING_SCREEN_UPLOAD_IMAGE &&
            event.argument_count==5 && event.argument[0]==Image &&
            event.argument[1]==x[i] && event.argument[2]==y[i] &&
            event.argument[3]==0 && event.argument[4]==0 &&
            event.saved_register[0]==Image &&
            event.saved_register[1]==Resource);
    }
    check(f.calls[9].kind==NBA97_GAME_LOADING_SCREEN_RELEASE_RESOURCE &&
        f.calls[9].argument_count==1 && f.calls[9].argument[0]==Resource);
}

void retail_null_behaviors() {
    Fixture missing;missing.resource=0;
    check(missing.run()==NBA97_TEXT_COMPLETE && missing.progress.completed &&
        missing.progress.skipped_for_null_resource &&
        !missing.progress.resource_loaded &&
        !missing.progress.image_lookup_completed &&
        !missing.progress.resolved_image_known);
    check(missing.calls.size()==1 && missing.progress.operations==7 &&
        missing.progress.accesses==6 && missing.progress.reads==3 &&
        missing.progress.stores==3 && missing.progress.callbacks_completed==1 &&
        missing.progress.load_calls==1 && !missing.progress.lookup_calls &&
        !missing.progress.draw_sync_calls && !missing.progress.upload_calls &&
        !missing.progress.release_calls && missing.progress.return_v0==0 &&
        missing.progress.return_v0_known);

    /* The original has no image-null check after 0x800A5478. */
    Fixture null_image;null_image.image=0;
    check(null_image.run()==NBA97_TEXT_COMPLETE &&
        null_image.progress.image_lookup_completed &&
        null_image.progress.resolved_image_known &&
        null_image.progress.resolved_image==0 &&
        null_image.progress.upload_calls==3);
    check(null_image.calls[3].argument[0]==0 &&
        null_image.calls[5].argument[0]==0 &&
        null_image.calls[7].argument[0]==0);
}

void unknownness_refusal_and_live_epilogue() {
    Fixture unknown_load;unknown_load.resource_known=0;
    check(unknown_load.run()==NBA97_TEXT_UNKNOWN &&
        unknown_load.progress.operations==4 &&
        unknown_load.progress.callbacks_completed==1 &&
        unknown_load.progress.stopped_pc==0x80029e7cu &&
        unknown_load.calls.size()==1);
    Fixture unknown_image;unknown_image.image_known=0;
    check(unknown_image.run()==NBA97_TEXT_UNKNOWN &&
        unknown_image.progress.operations==6 &&
        unknown_image.progress.callbacks_completed==3 &&
        unknown_image.progress.stopped_pc==0x80029ea0u &&
        unknown_image.calls.size()==3 &&
        unknown_image.calls[2].kind==NBA97_GAME_LOADING_SCREEN_DRAW_SYNC &&
        !unknown_image.calls[2].saved_register_known[0] &&
        unknown_image.calls[2].saved_register_known[1]);

    Fixture complete;check(complete.run()==NBA97_TEXT_COMPLETE);
    for(std::size_t i=0;i<complete.calls.size();++i) {
        Fixture refused;refused.refuse=i;
        check(refused.run()==NBA97_TEXT_IO_REFUSED &&
            refused.calls.size()==i+1 &&
            refused.progress.callbacks_completed==i &&
            refused.progress.stopped_pc==complete.calls[i].pc &&
            refused.progress.stopped_entry==complete.calls[i].entry);
    }
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED && no_io.calls.empty() &&
        no_io.progress.operations==4 && no_io.progress.stores==3 &&
        no_io.progress.stopped_entry==0x80029bfcu);
    Fixture malformed;malformed.malformed=4;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==5 &&
        malformed.progress.callbacks_completed==4 &&
        malformed.progress.stopped_entry==0x800994f4u);

    Fixture live;live.mutate_epilogue=true;
    check(live.run()==NBA97_TEXT_COMPLETE &&
        live.progress.restored_return_address==0x55667788u &&
        live.progress.restored_saved_register[0]==0xb1b2b3b4u &&
        live.progress.restored_saved_register[1]==0xa1a2a3a4u);
    Fixture unknown_stack;unknown_stack.unknown_epilogue=true;
    check(unknown_stack.run()==NBA97_TEXT_UNKNOWN &&
        unknown_stack.progress.operations==14 &&
        unknown_stack.progress.callbacks_completed==10 &&
        unknown_stack.progress.stopped_pc==0x80029f08u &&
        unknown_stack.progress.stopped_address==FrameSp+0x20u);
}

void budgets_and_memory_validation() {
    for(std::size_t budget=0;budget<16;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget);
    }
    Fixture exact;exact.context.operation_budget=16;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==16);
    Fixture missing;missing.context.memory={nullptr,0};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80029e68u);
    Fixture unaligned;unaligned.context.stack_pointer++;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80029e68u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.region,
        overlap.region};overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.region.size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT &&
        !null_regions.progress.operations);
    Fixture wraps;wraps.region.base=0xfffffffcu;wraps.region.size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameLoadingScreenProgress progress{};
    check(nba97_game_loading_screen(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_loading_screen(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    ordinary_path();
    retail_null_behaviors();
    unknownness_refusal_and_live_epilogue();
    budgets_and_memory_validation();
    std::printf("game_loading_screen: %u checks passed\n",checks);
}
