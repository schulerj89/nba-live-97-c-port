#include "recovered/game_cd_sync.h"

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
        std::fprintf(stderr,"game CD-sync check %u failed at line %u\n",
            checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029b3cu;
constexpr std::uint32_t Gp=0x800d79c8u;

struct Fixture {
    std::array<std::uint8_t,0x100> stack{},known{};
    Nba97GameTextRegion region{Stack,stack.data(),known.data(),stack.size()};
    Nba97GameCdSyncContext context{{&region,1},10,0,0,EntrySp,
        CallerRa,Gp,io,this};
    Nba97GameCdSyncProgress progress{};
    std::vector<Nba97GameCdSyncEvent> calls;
    Nba97GameCdSyncValue child{2,1};
    bool refuse=false;
    bool mutate_ra=false;
    bool unknown_ra=false;

    Fixture() {stack.fill(0xcd);known.fill(1);}
    void put(std::uint32_t address,std::uint32_t value) {
        const auto offset=address-region.base;
        for(unsigned i=0;i<4;++i) {
            region.data[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
            if(region.known)region.known[offset+i]=1;
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto offset=address-region.base;
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(region.data[offset+i])<<(i*8u);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameCdSyncEvent* event,Nba97GameCdSyncValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if(f.refuse)return 0;
        if(f.mutate_ra)f.put(FrameSp+0x10u,0x55667788u);
        if(f.unknown_ra)
            for(unsigned i=0;i<4;++i)
                f.known[FrameSp+0x10u-Stack+i]=0;
        *value=f.child;
        return 1;
    }
    int run() {return nba97_game_cd_sync(&context,&progress);}
};

void completed_and_forwarded() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==3 && f.progress.accesses==2 &&
        f.progress.reads==1 && f.progress.stores==1 &&
        f.progress.callbacks_completed==1);
    check(f.progress.mode==0 && f.progress.result_buffer==0 &&
        f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp && f.progress.global_pointer==Gp &&
        f.progress.service_entry==0x8009e740u &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.return_v0==2 && f.progress.return_v0_known);
    check(f.get(FrameSp+0x10u)==CallerRa);
    check(f.calls.size()==1 && f.calls[0].kind==NBA97_GAME_CD_SYNC_SERVICE &&
        f.calls[0].pc==0x8009dba8u &&
        f.calls[0].entry==0x8009e740u &&
        f.calls[0].argument_count==2 &&
        f.calls[0].argument[0]==0 && f.calls[0].argument[1]==0 &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].global_pointer==Gp &&
        f.calls[0].return_address==0x8009dbb0u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture poll;poll.context.mode=1;poll.context.result_buffer=0xdeadbeefu;
    poll.child={5,1};
    check(poll.run()==NBA97_TEXT_COMPLETE &&
        poll.calls[0].argument[0]==1 &&
        poll.calls[0].argument[1]==0xdeadbeefu &&
        poll.progress.return_v0==5);

    /* The wrapper never dereferences or validates result_buffer itself. */
    Fixture no_masks;no_masks.region.known=nullptr;
    no_masks.context.result_buffer=0xffffffffu;
    check(no_masks.run()==NBA97_TEXT_COMPLETE &&
        no_masks.calls[0].argument[1]==0xffffffffu);
}

void live_results_and_epilogue() {
    Fixture arbitrary;arbitrary.child={0x89abcdefu,1};
    check(arbitrary.run()==NBA97_TEXT_COMPLETE &&
        arbitrary.progress.return_v0==0x89abcdefu &&
        arbitrary.progress.return_v0_known);

    Fixture unknown;unknown.child={0x76543210u,0};
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.progress.completed &&
        unknown.progress.return_v0==0x76543210u &&
        !unknown.progress.return_v0_known);

    Fixture rewritten;rewritten.mutate_ra=true;
    check(rewritten.run()==NBA97_TEXT_COMPLETE &&
        rewritten.progress.restored_return_address==0x55667788u &&
        rewritten.get(FrameSp+0x10u)==0x55667788u);

    Fixture unknown_ra;unknown_ra.unknown_ra=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations==3 && unknown_ra.progress.accesses==2 &&
        unknown_ra.progress.stores==1 && !unknown_ra.progress.reads &&
        unknown_ra.progress.callbacks_completed==1 &&
        unknown_ra.progress.return_v0==2 &&
        unknown_ra.progress.return_v0_known &&
        unknown_ra.progress.stopped_pc==0x8009dbb0u &&
        unknown_ra.progress.stopped_address==FrameSp+0x10u);
}

void refusals_limits_and_validation() {
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations==2 && no_io.progress.accesses==1 &&
        no_io.progress.stores==1 && !no_io.progress.callbacks_completed &&
        no_io.progress.stopped_pc==0x8009dba8u &&
        no_io.progress.stopped_entry==0x8009e740u);
    Fixture refused;refused.refuse=true;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed && !refused.progress.completed);
    Fixture malformed;malformed.child.known=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed && !malformed.progress.completed);

    static constexpr std::uint32_t pcs[3]={0x8009dba4u,0x8009dba8u,
        0x8009dbb0u};
    for(std::size_t budget=0;budget<3;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.stopped_pc==pcs[budget] &&
            f.calls.size()==(budget>=2 ? 1u : 0u));
    }
    Fixture exact;exact.context.operation_budget=3;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==3);

    Fixture unaligned;++unaligned.context.stack_pointer;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x8009dba4u);
    Fixture missing;missing.context.memory={nullptr,0};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x8009dba4u);
    Fixture bad_known;bad_known.known[FrameSp+0x10u-Stack]=2;
    check(bad_known.run()==NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stopped_pc==0x8009dba4u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.region,overlap.region};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.region.size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Nba97GameCdSyncProgress progress{};
    check(nba97_game_cd_sync(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_cd_sync(&null_out.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    completed_and_forwarded();
    live_results_and_epilogue();
    refusals_limits_and_validation();
    std::printf("game_cd_sync: %u checks passed\n",checks);
}
