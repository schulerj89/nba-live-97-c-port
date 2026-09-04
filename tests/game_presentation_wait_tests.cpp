#include "recovered/game_presentation_wait.h"

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
        std::fprintf(stderr,"game presentation-wait check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029a6cu;
constexpr std::uint32_t Gp=0x800d79c8u;

struct Fixture {
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion region{Stack,stack.data(),stack_known.data(),stack.size()};
    Nba97GamePresentationWaitContext context{{&region,1},10,EntrySp,
        CallerRa,Gp,io,this};
    Nba97GamePresentationWaitProgress progress{};
    std::vector<Nba97GamePresentationWaitEvent> calls;
    std::uint32_t child_v0=1;
    bool child_v0_known=true;
    bool refuse=false;
    bool malformed=false;
    bool rewrite_saved=false;
    bool unknown_saved=false;

    Fixture() {stack.fill(0xcd);stack_known.fill(1);}
    void put(std::uint32_t address,std::uint32_t value) {
        const auto offset=address-region.base;
        for(unsigned i=0;i<4;++i) {
            region.data[offset+i]=std::uint8_t(value>>(8*i));
            if(region.known)region.known[offset+i]=1;
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto offset=address-region.base;
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(region.data[offset+i])<<(8*i);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GamePresentationWaitEvent* event,
        Nba97GamePresentationWaitValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if(f.refuse)return 0;
        if(f.rewrite_saved)f.put(event->stack_pointer+0x10u,0x2468ace0u);
        if(f.unknown_saved)
            for(unsigned i=0;i<4;++i)
                f.region.known[event->stack_pointer+0x10u-f.region.base+i]=0;
        *value={f.child_v0,static_cast<std::uint8_t>(f.child_v0_known)};
        if(f.malformed)value->known=2;
        return 1;
    }
    int run() {return nba97_game_presentation_wait(&context,&progress);}
};

void completed_wait() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==3 && f.progress.accesses==2 &&
        f.progress.reads==1 && f.progress.stores==1 &&
        f.progress.callbacks_completed==1);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp && f.progress.global_pointer==Gp &&
        f.progress.service_entry==0x800a9cc0u &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.return_v0==1 && f.progress.return_v0_known);
    check(f.get(FrameSp+0x10u)==CallerRa);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_PRESENTATION_WAIT_SERVICE &&
        f.calls[0].pc==0x80029be4u &&
        f.calls[0].entry==0x800a9cc0u &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].global_pointer==Gp &&
        f.calls[0].return_address==0x80029becu &&
        !f.calls[0].argument_count);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;without_masks.region.known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.progress.restored_return_address==CallerRa);
}

void live_child_results_and_epilogue() {
    Fixture arbitrary;arbitrary.child_v0=0x89abcdefu;
    check(arbitrary.run()==NBA97_TEXT_COMPLETE &&
        arbitrary.progress.return_v0==0x89abcdefu &&
        arbitrary.progress.return_v0_known);

    /* The original wrapper never validates or overwrites v0. Unknown child
       register state is therefore a completed, unknown wrapper result. */
    Fixture unknown;unknown.child_v0=0x76543210u;unknown.child_v0_known=false;
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.progress.completed &&
        unknown.progress.return_v0==0x76543210u &&
        !unknown.progress.return_v0_known);

    Fixture rewritten;rewritten.rewrite_saved=true;
    check(rewritten.run()==NBA97_TEXT_COMPLETE &&
        rewritten.progress.restored_return_address==0x2468ace0u &&
        rewritten.get(FrameSp+0x10u)==0x2468ace0u);

    Fixture unknown_ra;unknown_ra.unknown_saved=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations==3 && unknown_ra.progress.accesses==2 &&
        unknown_ra.progress.stores==1 && !unknown_ra.progress.reads &&
        unknown_ra.progress.callbacks_completed==1 &&
        unknown_ra.progress.return_v0==1 &&
        unknown_ra.progress.stopped_pc==0x80029becu);
}

void service_refusals() {
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations==2 && no_io.progress.accesses==1 &&
        no_io.progress.stores==1 && !no_io.progress.callbacks_completed &&
        no_io.progress.stopped_pc==0x80029be4u &&
        no_io.progress.stopped_entry==0x800a9cc0u);

    Fixture refused;refused.refuse=true;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed && !refused.progress.completed);

    Fixture malformed;malformed.malformed=true;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed && !malformed.progress.completed);
}

void limits_and_memory() {
    constexpr std::array<std::uint32_t,3> pcs={0x80029be0u,0x80029be4u,
        0x80029becu};
    for(std::size_t budget=0;budget<pcs.size();++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
        check(f.calls.size()==(budget<2?0u:1u));
        check(f.get(FrameSp+0x10u)==(budget?CallerRa:0xcdcdcdcdu));
    }

    Fixture unaligned;unaligned.context.stack_pointer=EntrySp+1u;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80029be0u);
    Fixture missing;missing.context.stack_pointer=0x90000020u;
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80029be0u);
    Fixture malformed_memory;malformed_memory.stack_known[FrameSp+0x10u-Stack]=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x80029be0u);

    Fixture overlap;
    Nba97GameTextRegion duplicate[2]={overlap.region,overlap.region};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;empty.region.size=0;
    check(empty.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GamePresentationWaitProgress progress{};
    check(nba97_game_presentation_wait(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_presentation_wait(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    completed_wait();
    live_child_results_and_epilogue();
    service_refusals();
    limits_and_memory();
    std::printf("game_presentation_wait: %u checks passed\n",checks);
}
