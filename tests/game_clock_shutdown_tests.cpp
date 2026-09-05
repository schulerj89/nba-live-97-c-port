#include "recovered/game_clock_shutdown.h"

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
        std::fprintf(stderr,"game clock-shutdown check %u failed at line %u\n",
            checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029b74u;
constexpr std::uint32_t IncomingFp=0xf7f7f7f7u;
constexpr std::uint32_t Gp=0x800d79c8u;
constexpr std::uint32_t Handler=0x800916b4u;

struct Fixture {
    std::array<std::uint8_t,0x100> stack{},known{};
    Nba97GameTextRegion region{Stack,stack.data(),known.data(),stack.size()};
    Nba97GameClockShutdownContext context{{&region,1},10,EntrySp,
        CallerRa,IncomingFp,Gp,io,this};
    Nba97GameClockShutdownProgress progress{};
    std::vector<Nba97GameClockShutdownEvent> calls;
    Nba97GameClockShutdownValue child{Handler,1};
    bool refuse=false;
    bool mutate_saved=false;
    bool unknown_ra=false;
    bool unknown_fp=false;

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
    void forget(std::uint32_t address) {
        for(unsigned i=0;i<4;++i)known[address-Stack+i]=0;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameClockShutdownEvent* event,
        Nba97GameClockShutdownValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if(f.refuse)return 0;
        if(f.mutate_saved) {
            f.put(FrameSp+0x14u,0x55667788u);
            f.put(FrameSp+0x10u,0x99aabbccu);
        }
        if(f.unknown_ra)f.forget(FrameSp+0x14u);
        if(f.unknown_fp)f.forget(FrameSp+0x10u);
        *value=f.child;
        return 1;
    }
    int run() {return nba97_game_clock_shutdown(&context,&progress);}
};

void removes_handler_and_restores_frame() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==5 && f.progress.accesses==4 &&
        f.progress.reads==2 && f.progress.stores==2 &&
        f.progress.callbacks_completed==1);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.incoming_frame_pointer==IncomingFp &&
        f.progress.global_pointer==Gp &&
        f.progress.interrupt_callback_entry==0x8009860cu &&
        f.progress.interrupt_number==6 &&
        f.progress.replacement_callback==0);
    check(f.progress.restored_return_address==CallerRa &&
        f.progress.restored_frame_pointer==IncomingFp &&
        f.progress.return_v0==Handler && f.progress.return_v0_known);
    check(f.get(FrameSp+0x14u)==CallerRa &&
        f.get(FrameSp+0x10u)==IncomingFp);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_CLOCK_SHUTDOWN_INTERRUPT_CALLBACK &&
        f.calls[0].pc==0x80091694u && f.calls[0].entry==0x8009860cu &&
        f.calls[0].argument_count==2 && f.calls[0].argument[0]==6 &&
        f.calls[0].argument[1]==0 && f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].frame_pointer==FrameSp &&
        f.calls[0].global_pointer==Gp &&
        f.calls[0].return_address==0x8009169cu);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture no_mask;no_mask.region.known=nullptr;
    check(no_mask.run()==NBA97_TEXT_COMPLETE &&
        no_mask.progress.restored_return_address==CallerRa &&
        no_mask.progress.restored_frame_pointer==IncomingFp);
}

void preserves_live_child_and_epilogue_values() {
    Fixture raw;raw.child={0xdeadbeefu,1};
    check(raw.run()==NBA97_TEXT_COMPLETE &&
        raw.progress.return_v0==0xdeadbeefu && raw.progress.return_v0_known);

    Fixture unknown;unknown.child={0x76543210u,0};
    check(unknown.run()==NBA97_TEXT_COMPLETE && unknown.progress.completed &&
        unknown.progress.return_v0==0x76543210u &&
        !unknown.progress.return_v0_known);

    Fixture rewritten;rewritten.mutate_saved=true;
    check(rewritten.run()==NBA97_TEXT_COMPLETE &&
        rewritten.progress.restored_return_address==0x55667788u &&
        rewritten.progress.restored_frame_pointer==0x99aabbccu);

    Fixture unknown_ra;unknown_ra.unknown_ra=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations==4 && unknown_ra.progress.accesses==3 &&
        !unknown_ra.progress.reads && unknown_ra.progress.stores==2 &&
        unknown_ra.progress.callbacks_completed==1 &&
        unknown_ra.progress.return_v0==Handler &&
        unknown_ra.progress.return_v0_known &&
        unknown_ra.progress.stopped_pc==0x800916a0u &&
        unknown_ra.progress.stopped_address==FrameSp+0x14u);

    Fixture unknown_fp;unknown_fp.unknown_fp=true;
    check(unknown_fp.run()==NBA97_TEXT_UNKNOWN &&
        unknown_fp.progress.operations==5 && unknown_fp.progress.accesses==4 &&
        unknown_fp.progress.reads==1 && unknown_fp.progress.stores==2 &&
        unknown_fp.progress.callbacks_completed==1 &&
        unknown_fp.progress.restored_return_address==CallerRa &&
        unknown_fp.progress.stopped_pc==0x800916a4u &&
        unknown_fp.progress.stopped_address==FrameSp+0x10u);
}

void refusals_limits_and_validation() {
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations==3 && no_io.progress.accesses==2 &&
        no_io.progress.stores==2 && !no_io.progress.callbacks_completed &&
        no_io.progress.stopped_pc==0x80091694u &&
        no_io.progress.stopped_entry==0x8009860cu);
    Fixture refused;refused.refuse=true;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed && !refused.progress.completed);
    Fixture malformed;malformed.child.known=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed && !malformed.progress.completed);

    static constexpr std::uint32_t pcs[5]={0x80091680u,0x80091684u,
        0x80091694u,0x800916a0u,0x800916a4u};
    for(std::size_t budget=0;budget<5;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.stopped_pc==pcs[budget] &&
            f.calls.size()==(budget>=3 ? 1u : 0u));
    }
    Fixture exact;exact.context.operation_budget=5;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==5);

    Fixture unaligned;++unaligned.context.stack_pointer;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80091680u);
    Fixture missing;missing.context.memory={nullptr,0};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80091680u);
    Fixture bad_known;bad_known.known[FrameSp+0x14u-Stack]=2;
    check(bad_known.run()==NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stopped_pc==0x80091680u);
    Fixture overlap;Nba97GameTextRegion duplicate[2]={overlap.region,overlap.region};
    overlap.context.memory={duplicate,2};
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.region.size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.region.data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;null_regions.context.memory={nullptr,1};
    check(null_regions.run()==NBA97_TEXT_ARGUMENT);
    Fixture wraps;wraps.region.base=0xfffffffcu;wraps.region.size=8;
    check(wraps.run()==NBA97_TEXT_ARGUMENT && !wraps.progress.operations);
    Nba97GameClockShutdownProgress progress{};
    check(nba97_game_clock_shutdown(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_clock_shutdown(&null_out.context,nullptr)==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    removes_handler_and_restores_frame();
    preserves_live_child_and_epilogue_values();
    refusals_limits_and_validation();
    std::printf("game_clock_shutdown: %u checks passed\n",checks);
}
