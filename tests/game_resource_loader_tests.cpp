#include "recovered/game_resource_loader.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game resource-loader check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffd0u;
constexpr std::uint32_t FrameSp = EntrySp - 0x20u;
constexpr std::uint32_t Filename = 0x800247ecu;
constexpr std::uint32_t Resource = 0x80123400u;

struct Fixture {
    std::array<std::uint8_t,0x100> stack{},known{};
    Nba97GameTextRegion region{Stack,stack.data(),known.data(),stack.size()};
    Nba97GameResourceLoaderContext context{{&region,1},100,Filename,0x20u,
        EntrySp,0x80029b04u,{0x11112222u,0x33334444u},0x800d79c8u,io,this};
    Nba97GameResourceLoaderProgress progress{};
    std::vector<Nba97GameResourceLoaderEvent> calls;
    std::size_t refuse=std::numeric_limits<std::size_t>::max();
    std::size_t malformed=std::numeric_limits<std::size_t>::max();
    std::size_t unknown=std::numeric_limits<std::size_t>::max();
    std::size_t zeros=0;
    bool always_zero=false;
    bool mutate_epilogue=false;
    bool unknown_epilogue=false;

    Fixture() {stack.fill(0xcd);known.fill(1);}
    void put(std::uint32_t address,std::uint32_t value) {
        const auto offset=address-Stack;
        for(unsigned i=0;i<4;++i) {
            stack[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
            known[offset+i]=1;
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto offset=address-Stack;
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(stack[offset+i])<<(i*8u);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameResourceLoaderEvent* event,
        Nba97GameResourceLoaderValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        const auto index=f.calls.size();
        f.calls.push_back(*event);
        if(index==f.refuse)return 0;
        if(f.mutate_epilogue) {
            f.put(FrameSp+0x18u,0x55667788u);
            f.put(FrameSp+0x14u,0xa1a2a3a4u);
            f.put(FrameSp+0x10u,0xb1b2b3b4u);
        }
        if(f.unknown_epilogue)
            f.known[FrameSp+0x18u-Stack]=0;
        if(index==f.unknown)*value={0x76543210u,0};
        else if(f.always_zero || index<f.zeros)*value={0,1};
        else *value={Resource,1};
        if(index==f.malformed)value->known=2;
        return 1;
    }
    int run() {return nba97_game_resource_loader(&context,&progress);}
};

void immediate_success() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==7 && f.progress.accesses==6 &&
        f.progress.reads==3 && f.progress.stores==3 &&
        f.progress.callbacks_completed==1 && f.progress.load_attempts==1 &&
        !f.progress.null_results);
    check(f.progress.filename==Filename && f.progress.flags==0x20u &&
        f.progress.return_v0==Resource && f.progress.return_v0_known);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.global_pointer==0x800d79c8u &&
        f.progress.restored_return_address==0x80029b04u &&
        f.progress.restored_saved_register[0]==0x11112222u &&
        f.progress.restored_saved_register[1]==0x33334444u);
    check(f.get(FrameSp+0x18u)==0x80029b04u &&
        f.get(FrameSp+0x14u)==0x33334444u &&
        f.get(FrameSp+0x10u)==0x11112222u);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_RESOURCE_LOADER_ATTEMPT &&
        f.calls[0].pc==0x80029c18u &&
        f.calls[0].entry==0x800941c8u &&
        f.calls[0].argument_count==2 &&
        f.calls[0].argument[0]==Filename &&
        f.calls[0].argument[1]==0x20u &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].global_pointer==0x800d79c8u &&
        f.calls[0].saved_register[0]==Filename &&
        f.calls[0].saved_register[1]==0x20u &&
        f.calls[0].saved_register_known[0] &&
        f.calls[0].saved_register_known[1] &&
        f.calls[0].return_address==0x80029c20u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture no_masks;no_masks.region.known=nullptr;
    check(no_masks.run()==NBA97_TEXT_COMPLETE &&
        no_masks.progress.return_v0==Resource);
}

void retries_and_live_stack() {
    Fixture f;f.zeros=3;f.mutate_epilogue=true;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.calls.size()==4 && f.progress.operations==10 &&
        f.progress.callbacks_completed==4 && f.progress.load_attempts==4 &&
        f.progress.null_results==3 && f.progress.return_v0==Resource);
    for(const auto& event:f.calls)
        check(event.pc==0x80029c18u && event.entry==0x800941c8u &&
            event.argument[0]==Filename && event.argument[1]==0x20u &&
            event.saved_register[0]==Filename &&
            event.saved_register[1]==0x20u);
    check(f.progress.restored_return_address==0x55667788u &&
        f.progress.restored_saved_register[0]==0xb1b2b3b4u &&
        f.progress.restored_saved_register[1]==0xa1a2a3a4u);

    Fixture bounded;bounded.always_zero=true;bounded.context.operation_budget=8;
    check(bounded.run()==NBA97_TEXT_LIMIT && !bounded.progress.completed &&
        bounded.progress.operations==8 && bounded.calls.size()==5 &&
        bounded.progress.callbacks_completed==5 &&
        bounded.progress.load_attempts==5 && bounded.progress.null_results==5 &&
        bounded.progress.stopped_pc==0x80029c18u &&
        bounded.progress.stopped_entry==0x800941c8u);
}

void unknownness_and_refusal() {
    Fixture unknown;unknown.unknown=0;
    check(unknown.run()==NBA97_TEXT_UNKNOWN && unknown.calls.size()==1 &&
        unknown.progress.operations==4 &&
        unknown.progress.callbacks_completed==1 &&
        unknown.progress.load_attempts==1 && !unknown.progress.null_results &&
        unknown.progress.stopped_pc==0x80029c20u);

    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED && no_io.calls.empty() &&
        no_io.progress.operations==4 && no_io.progress.stores==3 &&
        !no_io.progress.callbacks_completed &&
        no_io.progress.stopped_pc==0x80029c18u &&
        no_io.progress.stopped_entry==0x800941c8u);
    Fixture refused;refused.refuse=0;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed && !refused.progress.completed);
    Fixture malformed;malformed.malformed=0;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed && !malformed.progress.completed);

    Fixture unknown_stack;unknown_stack.unknown_epilogue=true;
    check(unknown_stack.run()==NBA97_TEXT_UNKNOWN &&
        unknown_stack.progress.operations==5 &&
        unknown_stack.progress.callbacks_completed==1 &&
        unknown_stack.progress.return_v0==Resource &&
        unknown_stack.progress.return_v0_known &&
        unknown_stack.progress.stopped_pc==0x80029c28u &&
        unknown_stack.progress.stopped_address==FrameSp+0x18u);
}

void budgets_and_memory_validation() {
    static constexpr std::uint32_t pcs[7]={0x80029c00u,0x80029c08u,
        0x80029c10u,0x80029c18u,0x80029c28u,0x80029c2cu,0x80029c30u};
    for(std::size_t budget=0;budget<7;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget && f.progress.stopped_pc==pcs[budget]);
        check(f.calls.size()==(budget>=4?1u:0u));
    }
    Fixture exact;exact.context.operation_budget=7;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==7);
    Fixture missing;missing.context.memory={nullptr,0};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80029c00u);
    Fixture unaligned;unaligned.context.stack_pointer++;
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80029c00u);
    Fixture malformed_memory;
    malformed_memory.known[FrameSp+0x10u-Stack]=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x80029c00u);
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
    Nba97GameResourceLoaderProgress progress{};
    check(nba97_game_resource_loader(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_resource_loader(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    immediate_success();
    retries_and_live_stack();
    unknownness_and_refusal();
    budgets_and_memory_validation();
    std::printf("game_resource_loader: %u checks passed\n",checks);
}
