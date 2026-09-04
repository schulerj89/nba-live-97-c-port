#include "recovered/game_clock_delta.h"

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
        std::fprintf(stderr,"game clock-delta check %u failed\n",checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram=0x80000000u;
constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t CallerRa=0x80029a64u;
constexpr std::uint32_t SavedS0=0x13579bdfu;
constexpr std::uint32_t Gp=0x800d79c8u;
constexpr std::uint32_t Snapshot=0x800d7b2cu;

struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x200000,0xcd);
    std::vector<std::uint8_t> ram_known=std::vector<std::uint8_t>(0x200000,1);
    std::array<std::uint8_t,0x100> stack{};
    std::array<std::uint8_t,0x100> stack_known{};
    Nba97GameTextRegion regions[2]={{Ram,ram.data(),ram_known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}};
    Nba97GameClockDeltaContext context{{regions,2},20,EntrySp,CallerRa,
        SavedS0,Gp,io,this};
    Nba97GameClockDeltaProgress progress{};
    std::vector<Nba97GameClockDeltaEvent> calls;
    std::uint32_t sample=135;
    std::uint32_t refuse_entry=0;
    bool sample_known=true;
    bool malformed=false;
    bool rewrite_snapshot=false;
    bool rewrite_saved=false;
    bool unknown_saved_ra=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);put(Snapshot,100);
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
    void put(std::uint32_t address,std::uint32_t value) {
        for(unsigned i=0;i<4;++i) {
            *byte(address+i)=std::uint8_t(value>>(8*i));
            if(auto* mask=known(address+i))*mask=1;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)value|=std::uint32_t(*byte(address+i))<<(8*i);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameClockDeltaEvent* event,Nba97GameClockDeltaValue* value) {
        auto& f=*static_cast<Fixture*>(user);f.calls.push_back(*event);
        if(event->entry==f.refuse_entry)return 0;
        if(f.rewrite_snapshot)f.put(f.context.global_pointer+0x164u,0xdeadbeefu);
        if(f.rewrite_saved) {
            f.put(event->stack_pointer+0x10u,0x2468ace0u);
            f.put(event->stack_pointer+0x14u,0x10203040u);
        }
        if(f.unknown_saved_ra)
            for(unsigned i=0;i<4;++i)*f.known(event->stack_pointer+0x14u+i)=0;
        *value={f.sample,static_cast<std::uint8_t>(f.sample_known)};
        if(f.malformed)value->known=2;
        return 1;
    }
    int run() {return nba97_game_clock_delta(&context,&progress);}
};

void forward_sample() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==7 && f.progress.accesses==6 &&
        f.progress.reads==3 && f.progress.stores==3 &&
        f.progress.callbacks_completed==1);
    check(f.progress.global_pointer==Gp && f.progress.snapshot_address==Snapshot &&
        f.progress.previous_snapshot==100 && f.progress.sampled_clock==135 &&
        f.progress.sampled_clock_known && f.progress.return_v0==35 &&
        f.progress.return_v0_known);
    check(f.get(Snapshot)==135 && f.get(FrameSp+0x10u)==SavedS0 &&
        f.get(FrameSp+0x14u)==CallerRa);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.restored_return_address==CallerRa &&
        f.progress.restored_saved_register_s0==SavedS0);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_CLOCK_DELTA_READ_CLOCK &&
        f.calls[0].pc==0x800a585cu && f.calls[0].entry==0x800a5810u &&
        f.calls[0].stack_pointer==FrameSp && f.calls[0].global_pointer==Gp &&
        f.calls[0].return_address==0x800a5864u &&
        !f.calls[0].argument_count);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture without_masks;
    without_masks.regions[0].known=nullptr;
    without_masks.regions[1].known=nullptr;
    check(without_masks.run()==NBA97_TEXT_COMPLETE &&
        without_masks.get(Snapshot)==without_masks.sample &&
        without_masks.progress.return_v0==35);
}

void raw_wrap_and_repeat() {
    Fixture wrap;wrap.put(Snapshot,0xfffffff0u);wrap.sample=0x10u;
    check(wrap.run()==NBA97_TEXT_COMPLETE && wrap.progress.return_v0==0x20u &&
        wrap.get(Snapshot)==0x10u);
    Fixture backward;backward.put(Snapshot,100);backward.sample=90;
    check(backward.run()==NBA97_TEXT_COMPLETE &&
        backward.progress.return_v0==0xfffffff6u && backward.get(Snapshot)==90);
    backward.sample=125;
    check(backward.run()==NBA97_TEXT_COMPLETE &&
        backward.progress.previous_snapshot==90 &&
        backward.progress.return_v0==35 && backward.get(Snapshot)==125);
}

void live_source_order() {
    Fixture child_write;child_write.rewrite_snapshot=true;child_write.sample=150;
    check(child_write.run()==NBA97_TEXT_COMPLETE &&
        child_write.progress.previous_snapshot==100 &&
        child_write.progress.return_v0==50 && child_write.get(Snapshot)==150);

    Fixture saved_write;saved_write.rewrite_saved=true;
    check(saved_write.run()==NBA97_TEXT_COMPLETE &&
        saved_write.progress.restored_saved_register_s0==0x2468ace0u &&
        saved_write.progress.restored_return_address==0x10203040u);

    /* Deliberate alias: the s0 spill becomes the prior sample, then the new
       snapshot becomes the word restored into s0. Preserve source order. */
    Fixture alias;alias.context.stack_pointer=Snapshot+8u;
    alias.context.saved_register_s0=0x20u;alias.sample=0x30u;
    check(alias.run()==NBA97_TEXT_COMPLETE &&
        alias.progress.frame_stack_pointer==Snapshot-0x10u &&
        alias.progress.previous_snapshot==0x20u &&
        alias.progress.return_v0==0x10u && alias.get(Snapshot)==0x30u &&
        alias.progress.restored_saved_register_s0==0x30u);
}

void unknown_and_refusals() {
    Fixture unknown_old;
    for(unsigned i=0;i<4;++i)*unknown_old.known(Snapshot+i)=0;
    check(unknown_old.run()==NBA97_TEXT_UNKNOWN &&
        unknown_old.progress.operations==2 && unknown_old.progress.accesses==2 &&
        unknown_old.progress.stores==1 && !unknown_old.progress.reads &&
        unknown_old.calls.empty() && unknown_old.progress.stopped_pc==0x800a5854u);

    Fixture unknown_sample;unknown_sample.sample=0x89abcdefu;
    unknown_sample.sample_known=false;
    check(unknown_sample.run()==NBA97_TEXT_COMPLETE && unknown_sample.progress.completed &&
        unknown_sample.progress.sampled_clock==0x89abcdefu &&
        !unknown_sample.progress.sampled_clock_known &&
        unknown_sample.progress.return_v0==0x89abcd8bu &&
        !unknown_sample.progress.return_v0_known &&
        unknown_sample.get(Snapshot)==0x89abcdefu);
    for(unsigned i=0;i<4;++i)check(*unknown_sample.known(Snapshot+i)==0);

    Fixture unrepresentable;unrepresentable.sample_known=false;
    unrepresentable.regions[0].known=nullptr;
    unrepresentable.regions[1].known=nullptr;
    check(unrepresentable.run()==NBA97_TEXT_ARGUMENT &&
        unrepresentable.progress.operations==5 &&
        unrepresentable.progress.callbacks_completed==1 &&
        unrepresentable.progress.stores==2 &&
        unrepresentable.progress.stopped_pc==0x800a5864u);

    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations==4 && no_io.progress.stopped_pc==0x800a585cu &&
        no_io.progress.stopped_entry==0x800a5810u);
    Fixture refused;refused.refuse_entry=0x800a5810u;
    check(refused.run()==NBA97_TEXT_IO_REFUSED && refused.calls.size()==1 &&
        !refused.progress.callbacks_completed && refused.get(Snapshot)==100);
    Fixture malformed;malformed.malformed=true;
    check(malformed.run()==NBA97_TEXT_ARGUMENT && malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed && malformed.get(Snapshot)==100);

    Fixture unknown_ra;unknown_ra.unknown_saved_ra=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations==6 && unknown_ra.progress.stores==3 &&
        unknown_ra.get(Snapshot)==unknown_ra.sample &&
        unknown_ra.progress.stopped_pc==0x800a586cu);
}

void limits_and_memory() {
    constexpr std::array<std::uint32_t,7> pcs={0x800a5850u,0x800a5854u,
        0x800a5858u,0x800a585cu,0x800a5864u,0x800a586cu,0x800a5870u};
    for(std::size_t budget=0;budget<pcs.size();++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] && !f.progress.completed);
        check(f.get(Snapshot)==(budget<5?100u:f.sample));
        check(f.calls.size()==(budget<4?0u:1u));
    }

    Fixture unaligned_stack;unaligned_stack.context.stack_pointer=EntrySp+1u;
    check(unaligned_stack.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_stack.progress.stopped_pc==0x800a5850u);
    Fixture unaligned_gp;unaligned_gp.context.global_pointer=Gp+1u;
    check(unaligned_gp.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_gp.progress.stopped_pc==0x800a5854u);
    Fixture missing_stack;missing_stack.context.stack_pointer=0x90000020u;
    check(missing_stack.run()==NBA97_TEXT_RESOURCE &&
        missing_stack.progress.stopped_pc==0x800a5850u);
    Fixture missing_snapshot;missing_snapshot.context.global_pointer=0x90000000u;
    check(missing_snapshot.run()==NBA97_TEXT_RESOURCE &&
        missing_snapshot.progress.stopped_pc==0x800a5854u);
    Fixture malformed_memory;*malformed_memory.known(Snapshot)=2;
    check(malformed_memory.run()==NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc==0x800a5854u);

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
    Nba97GameClockDeltaProgress progress{};
    check(nba97_game_clock_delta(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_clock_delta(&f.context,nullptr)==NBA97_TEXT_ARGUMENT);
}
}

int main() {
    forward_sample();
    raw_wrap_and_repeat();
    live_source_order();
    unknown_and_refusals();
    limits_and_memory();
    std::printf("game_clock_delta: %u checks passed\n",checks);
}
