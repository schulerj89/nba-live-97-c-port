#include "recovered/game_heap_payload_size.h"
#include "recovered/game_heap_release.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value,unsigned line) {
    ++checks;
    if(!value) {
        std::fprintf(stderr,"game heap payload-size check %u failed at line %u\n",
            checks,line);
        std::exit(1);
    }
}
#define check(value) checkAt((value),__LINE__)

constexpr std::uint32_t Stack=0x807fff00u;
constexpr std::uint32_t EntrySp=0x807fffd0u;
constexpr std::uint32_t FrameSp=EntrySp-0x18u;
constexpr std::uint32_t Descriptor=0x80110000u;
constexpr std::uint32_t Payload=0x80123400u;
constexpr std::uint32_t RequestedSize=0x1410u;

struct Fixture {
    std::array<std::uint8_t,0x100> stack{},stack_known{};
    std::array<std::uint8_t,0x80> descriptor{},descriptor_known{};
    std::array<std::uint8_t,0x80> low{},low_known{};
    std::array<Nba97GameTextRegion,3> regions{{
        {Stack,stack.data(),stack_known.data(),stack.size()},
        {Descriptor,descriptor.data(),descriptor_known.data(),descriptor.size()},
        {0,low.data(),low_known.data(),low.size()}}};
    Nba97GameHeapPayloadSizeContext context{{regions.data(),regions.size()},
        100,Payload,EntrySp,0x80029b10u,0x800d79c8u,io,this};
    Nba97GameHeapPayloadSizeProgress progress{};
    std::vector<Nba97GameHeapPayloadSizeEvent> calls;
    Nba97GameHeapPayloadSizeValue child{Descriptor,1};
    bool refuse=false;
    bool mutate=false;
    bool unknown_size=false;
    bool unknown_ra=false;

    Fixture() {
        stack.fill(0xcd);stack_known.fill(1);
        descriptor.fill(0xcd);descriptor_known.fill(1);
        low.fill(0xcd);low_known.fill(1);
        put(Descriptor+0x14u,RequestedSize);
        put(0x14u,0x10203040u);
        put(4u,0x50607080u);
    }
    std::pair<std::uint8_t*,std::uint8_t*> byte(std::uint32_t address) {
        for(auto& region:regions)
            if(address>=region.base &&
               std::uint64_t(address-region.base)<region.size)
                return {region.data+(address-region.base),
                    region.known ? region.known+(address-region.base) : nullptr};
        check(false);return {nullptr,nullptr};
    }
    void put(std::uint32_t address,std::uint32_t value) {
        for(unsigned i=0;i<4;++i) {
            auto target=byte(address+i);
            *target.first=static_cast<std::uint8_t>(value>>(i*8u));
            if(target.second)*target.second=1;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
            value|=std::uint32_t(*byte(address+i).first)<<(i*8u);
        return value;
    }
    static int io(void* user,const Nba97GameTextMemory*,
        const Nba97GameHeapPayloadSizeEvent* event,
        Nba97GameHeapPayloadSizeValue* value) {
        auto& f=*static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if(f.refuse)return 0;
        if(f.mutate) {
            f.put(Descriptor+0x14u,0xa1b2c3d4u);
            f.put(FrameSp+0x10u,0x55667788u);
        }
        if(f.unknown_size)
            *f.byte(Descriptor+0x14u).second=0;
        if(f.unknown_ra)
            *f.byte(FrameSp+0x10u).second=0;
        *value=f.child;
        return 1;
    }
    int run() {return nba97_game_heap_payload_size(&context,&progress);}
};

void ordinary_and_live_reads() {
    Fixture f;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations==4 && f.progress.accesses==3 &&
        f.progress.reads==2 && f.progress.stores==1 &&
        f.progress.callbacks_completed==1 &&
        f.progress.descriptor_lookup_calls==1);
    check(f.progress.payload==Payload &&
        f.progress.descriptor==Descriptor && f.progress.descriptor_known &&
        f.progress.requested_size==RequestedSize &&
        f.progress.return_v0==RequestedSize && f.progress.return_v0_known);
    check(f.progress.frame_stack_pointer==FrameSp &&
        f.progress.stack_pointer==EntrySp &&
        f.progress.global_pointer==0x800d79c8u &&
        f.progress.restored_return_address==0x80029b10u);
    check(f.get(FrameSp+0x10u)==0x80029b10u);
    check(f.calls.size()==1 &&
        f.calls[0].kind==NBA97_GAME_HEAP_PAYLOAD_SIZE_FIND_DESCRIPTOR &&
        f.calls[0].pc==0x80090d68u &&
        f.calls[0].entry==0x80090618u &&
        f.calls[0].argument_count==1 &&
        f.calls[0].argument[0]==Payload &&
        f.calls[0].stack_pointer==FrameSp &&
        f.calls[0].global_pointer==0x800d79c8u &&
        f.calls[0].return_address==0x80090d70u);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture live;live.mutate=true;
    check(live.run()==NBA97_TEXT_COMPLETE && live.progress.completed &&
        live.progress.return_v0==0xa1b2c3d4u &&
        live.progress.restored_return_address==0x55667788u);

    Fixture no_masks;
    for(auto& region:no_masks.regions)region.known=nullptr;
    check(no_masks.run()==NBA97_TEXT_COMPLETE &&
        no_masks.progress.return_v0==RequestedSize);
}

void source_pointer_quirks() {
    Fixture null_descriptor;null_descriptor.child={0,1};
    check(null_descriptor.run()==NBA97_TEXT_COMPLETE &&
        null_descriptor.progress.completed &&
        null_descriptor.progress.descriptor==0 &&
        null_descriptor.progress.descriptor_known &&
        null_descriptor.progress.return_v0==0x10203040u);

    Fixture wrapped;wrapped.child={0xfffffff0u,1};
    check(wrapped.run()==NBA97_TEXT_COMPLETE &&
        wrapped.progress.return_v0==0x50607080u);

    Fixture unaligned;unaligned.child={Descriptor+1u,1};
    check(unaligned.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc==0x80090d70u &&
        unaligned.progress.stopped_address==Descriptor+0x15u);

    Fixture missing_low;missing_low.child={0,1};
    missing_low.context.memory.count=2;
    check(missing_low.run()==NBA97_TEXT_RESOURCE &&
        missing_low.progress.stopped_pc==0x80090d70u &&
        missing_low.progress.stopped_address==0x14u);
}

void unknownness_and_refusal() {
    Fixture unknown_child;unknown_child.child={Descriptor,0};
    check(unknown_child.run()==NBA97_TEXT_UNKNOWN &&
        unknown_child.calls.size()==1 &&
        unknown_child.progress.operations==2 &&
        unknown_child.progress.callbacks_completed==1 &&
        !unknown_child.progress.descriptor_known &&
        unknown_child.progress.stopped_pc==0x80090d70u);

    Fixture malformed;malformed.child.known=2;
    check(malformed.run()==NBA97_TEXT_ARGUMENT &&
        malformed.calls.size()==1 &&
        !malformed.progress.callbacks_completed);
    Fixture refused;refused.refuse=true;
    check(refused.run()==NBA97_TEXT_IO_REFUSED &&
        refused.calls.size()==1 && !refused.progress.callbacks_completed);
    Fixture no_io;no_io.context.io=nullptr;
    check(no_io.run()==NBA97_TEXT_IO_REFUSED && no_io.calls.empty() &&
        no_io.progress.operations==2 &&
        no_io.progress.stopped_pc==0x80090d68u &&
        no_io.progress.stopped_entry==0x80090618u);

    Fixture unknown_size;unknown_size.unknown_size=true;
    check(unknown_size.run()==NBA97_TEXT_UNKNOWN &&
        unknown_size.progress.stopped_pc==0x80090d70u &&
        unknown_size.progress.stopped_address==Descriptor+0x14u);
    Fixture unknown_ra;unknown_ra.unknown_ra=true;
    check(unknown_ra.run()==NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.return_v0==RequestedSize &&
        unknown_ra.progress.return_v0_known &&
        unknown_ra.progress.stopped_pc==0x80090d74u &&
        unknown_ra.progress.stopped_address==FrameSp+0x10u);
}

void budgets_and_validation() {
    static constexpr std::uint32_t pcs[4]={0x80090d64u,0x80090d68u,
        0x80090d70u,0x80090d74u};
    for(std::size_t budget=0;budget<4;++budget) {
        Fixture f;f.context.operation_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations==budget &&
            f.progress.stopped_pc==pcs[budget] &&
            f.calls.size()==(budget>=2 ? 1u : 0u));
    }
    Fixture exact;exact.context.operation_budget=4;
    check(exact.run()==NBA97_TEXT_COMPLETE && exact.progress.operations==4);

    Fixture unaligned_stack;++unaligned_stack.context.stack_pointer;
    check(unaligned_stack.run()==NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_stack.progress.stopped_pc==0x80090d64u);
    Fixture bad_known;
    *bad_known.byte(FrameSp+0x10u).second=2;
    check(bad_known.run()==NBA97_TEXT_ARGUMENT &&
        bad_known.progress.stopped_pc==0x80090d64u);
    Fixture overlap;overlap.regions[2]=overlap.regions[1];
    check(overlap.run()==NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture zero;zero.regions[1].size=0;
    check(zero.run()==NBA97_TEXT_ARGUMENT && !zero.progress.operations);
    Fixture null_data;null_data.regions[1].data=nullptr;
    check(null_data.run()==NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture missing;missing.context.memory={nullptr,0};
    check(missing.run()==NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc==0x80090d64u);

    Nba97GameHeapPayloadSizeProgress progress{};
    check(nba97_game_heap_payload_size(nullptr,&progress)==NBA97_TEXT_ARGUMENT);
    Fixture null_out;
    check(nba97_game_heap_payload_size(&null_out.context,nullptr)==
        NBA97_TEXT_ARGUMENT);
}

struct ComposedFixture {
    static constexpr std::uint32_t Ram=0x80000000u;
    static constexpr std::uint32_t Low=0x80110000u;
    static constexpr std::uint32_t Node=Low+0x28u;
    static constexpr std::uint32_t High=Low+0x50u;
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x120000,0xcd);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x120000,1);
    std::array<std::uint8_t,0x100> stack{},stack_known{};
    std::array<Nba97GameTextRegion,2> regions{{
        {Ram,ram.data(),known.data(),ram.size()},
        {Stack,stack.data(),stack_known.data(),stack.size()}}};
    Nba97GameHeapPayloadSizeContext context{{regions.data(),regions.size()},
        20,Payload,EntrySp,0x80029b10u,0x800d79c8u,io,this};
    Nba97GameHeapPayloadSizeProgress progress{};
    Nba97GameHeapReleaseProgress lookup{};
    std::vector<Nba97GameHeapPayloadSizeEvent> calls;

    ComposedFixture() {
        stack.fill(0xcd);stack_known.fill(1);
        for(unsigned bank=0;bank<16;++bank)put(0x80103d54u+bank*24u,0);
        put(0x80103d50u,Low);put(0x80103d54u,High);
        put(Low+0x20u,Node);
        put(Node,Payload);put(Node+0x14u,RequestedSize);
        put(Node+0x18u,0);put(Node+0x20u,High);put(Node+0x24u,Low);
        put(High,0x801fd800u);put(High+0x18u,0x8000u);
        put(High+0x24u,Node);
    }
    void put(std::uint32_t address,std::uint32_t value) {
        Nba97GameTextRegion* region=address>=Stack ? &regions[1] : &regions[0];
        const auto offset=address-region->base;
        for(unsigned i=0;i<4;++i) {
            region->data[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
            region->known[offset+i]=1;
        }
    }
    static int io(void* user,const Nba97GameTextMemory* memory,
        const Nba97GameHeapPayloadSizeEvent* event,
        Nba97GameHeapPayloadSizeValue* value) {
        auto& f=*static_cast<ComposedFixture*>(user);
        f.calls.push_back(*event);
        Nba97GameHeapReleaseContext heap{*memory,100};
        if(nba97_game_heap_release(&heap,NBA97_HEAP_FIND_90618,
               event->argument[0],{0,0},nullptr,0,&f.lookup)!=
                   NBA97_TEXT_COMPLETE || !f.lookup.completed)
            return 0;
        *value={f.lookup.returned.word,f.lookup.returned.known};
        return 1;
    }
};

void composed_heap_lookup() {
    ComposedFixture f;
    check(nba97_game_heap_payload_size(&f.context,&f.progress)==
        NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.calls.size()==1 && f.lookup.completed &&
        f.lookup.accesses==5 && !f.lookup.stores &&
        f.lookup.descriptor==ComposedFixture::Node &&
        f.lookup.returned.known &&
        f.lookup.returned.word==ComposedFixture::Node);
    check(f.progress.descriptor==ComposedFixture::Node &&
        f.progress.return_v0==RequestedSize);
}
}

int main() {
    ordinary_and_live_reads();
    source_pointer_quirks();
    unknownness_and_refusal();
    budgets_and_validation();
    composed_heap_lookup();
    std::printf("game_heap_payload_size: %u checks passed\n",checks);
}
